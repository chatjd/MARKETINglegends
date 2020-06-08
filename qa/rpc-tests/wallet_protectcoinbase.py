#!/usr/bin/env python2
# Copyright (c) 2016 The Zcash developers
# Copyright (c) 2017-2018 The SnowGem developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, initialize_chain_clean, \
    start_nodes, connect_nodes_bi, stop_node, wait_and_assert_operationid_status

import sys
import time
import timeit
from decimal import Decimal

def check_value_pool(node, name, total):
    value_pools = node.getblockchaininfo()['valuePools']
    found = False
    for pool in value_pools:
        if pool['id'] == name:
            found = True
            assert_equal(pool['monitored'], True)
            assert_equal(pool['chainValue'], total)
    assert(found)

class WalletProtectCoinbaseTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    # Start nodes with -regtestprotectcoinbase to set fCoinbaseMustBeProtected to true.
    def setup_network(self, split=False):
        self.nodes = start_nodes(4, self.options.tmpdir, extra_args=[['-regtestprotectcoinbase', '-debug=zrpcunsafe']] * 4 )
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        connect_nodes_bi(self.nodes,0,3)
        self.is_network_split=False
        self.sync_all()

    # Returns txid if operation was a success or None
    def wait_and_assert_operationid_status(self, myopid, in_status='success', in_errormsg=None):
        print('waiting for async operation {}'.format(myopid))
        opids = []
        opids.append(myopid)
        timeout = 300
        status = None
        errormsg = None
        txid = None
        for x in xrange(1, timeout):
            results = self.nodes[0].z_getoperationresult(opids)
            if len(results)==0:
                time.sleep(1)
            else:
                status = results[0]["status"]
                if status == "failed":
                    errormsg = results[0]['error']['message']
                elif status == "success":
                    txid = results[0]['result']['txid']
                break
        print('...returned status: {}'.format(status))
        assert_equal(in_status, status)
        if errormsg is not None:
            assert(in_errormsg is not None)
            assert_equal(in_errormsg in errormsg, True)
            print('...returned error: {}'.format(errormsg))
        return txid

    def run_test (self):
        print "Mining blocks..."

        self.nodes[0].generate(4)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 40)
        assert_equal(walletinfo['balance'], 0)

        self.sync_all()
        self.nodes[1].generate(101)
        self.sync_all()

        assert_equal(self.nodes[0].getbalance(), 40)
        assert_equal(self.nodes[1].getbalance(), 10)
        assert_equal(self.nodes[2].getbalance(), 0)
        assert_equal(self.nodes[3].getbalance(), 0)

        check_value_pool(self.nodes[0], 'sprout', 0)
        check_value_pool(self.nodes[1], 'sprout', 0)
        check_value_pool(self.nodes[2], 'sprout', 0)
        check_value_pool(self.nodes[3], 'sprout', 0)

        # Send will fail because we are enforcing the consensus rule that
        # coinbase utxos can only be sent to a zaddr.
        errorString = ""
        try:
            self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 1)
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("Coinbase funds can only be sent to a zaddr" in errorString, True)

        # Prepare to send taddr->zaddr
        mytaddr = self.nodes[0].getnewaddress()
        myzaddr = self.nodes[0].z_getnewaddress()

        # Node 3 will test that watch only address utxos are not selected
        self.nodes[3].importaddress(mytaddr)
        recipients= [{"address":myzaddr, "amount": Decimal('1')}]
        myopid = self.nodes[3].z_sendmany(mytaddr, recipients)
        errorString=""
        status = None
        opids = [myopid]
        timeout = 10
        for x in xrange(1, timeout):
            results = self.nodes[3].z_getoperationresult(opids)
            if len(results)==0:
                time.sleep(1)
            else:
                status = results[0]["status"]
                errorString = results[0]["error"]["message"]
                break
        assert_equal("failed", status)
        assert_equal("no UTXOs found for taddr from address" in errorString, True)
        stop_node(self.nodes[3], 3)
        self.nodes.pop()

        # This send will fail because our wallet does not allow any change when protecting a coinbase utxo,
        # as it's currently not possible to specify a change address in z_sendmany.
        recipients = []
        recipients.append({"address":myzaddr, "amount":Decimal('1.23456789')})
        errorString = ""
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        opids = []
        opids.append(myopid)
        timeout = 10
        status = None
        for x in xrange(1, timeout):
            results = self.nodes[0].z_getoperationresult(opids)
            if len(results)==0:
                time.sleep(1)
            else:
                status = results[0]["status"]
                errorString = results[0]["error"]["message"]

                # Test that the returned status object contains a params field with the operation's input parameters
                assert_equal(results[0]["method"], "z_sendmany")
                params =results[0]["params"]
                assert_equal(params["fee"], Decimal('0.0001')) # default
                assert_equal(params["minconf"], Decimal('1')) # default
                assert_equal(params["fromaddress"], mytaddr)
                assert_equal(params["amounts"][0]["address"], myzaddr)
                assert_equal(params["amounts"][0]["amount"], Decimal('1.23456789'))
                break
        assert_equal("failed", status)
        assert_equal("wallet does not allow any change" in errorString, True)

        # This send will succeed.  We send two coinbase utxos totalling 20.0 less a fee of 0.00010000, with no change.
        shieldvalue = Decimal('20.0') - Decimal('0.0001')
        recipients = []
        recipients.append({"address":myzaddr, "amount": shieldvalue})
        myopid = self.nodes[0].z_sendmany(mytaddr, recipients)
        mytxid = wait_and_assert_operationid_status(self.nodes[0], myopid)
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        # Verify that debug=zrpcunsafe logs params, and that full txid is associated with opid
        logpath = self.options.tmpdir+"/node0/regtest/debug.log"
        logcounter = 0
        with open(logpath, "r") as myfile:
            logdata = myfile.readlines()
        f