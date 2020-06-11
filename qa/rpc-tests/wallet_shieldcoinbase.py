#!/usr/bin/env python2
# Copyright (c) 2017 The Zcash developers
# Copyright (c) 2017-2018 The SnowGem developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, initialize_chain_clean, \
    start_node, connect_nodes_bi, sync_blocks, sync_mempools, \
    wait_and_assert_operationid_status

from decimal import Decimal

class WalletShieldCoinbaseTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_network(self, split=False):
        args = ['-regtestprotectcoinbase', '-debug=zrpcunsafe']
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, args))
        self.nodes.append(start_node(1, self.options.tmpdir, args))
        args2 = ['-regtestprotectcoinbase', '-debug=zrpcunsafe', "-mempooltxinputlimit=7"]
        self.nodes.append(start_node(2, self.options.tmpdir, args2))
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.is_network_split=False
        self.sync_all()

    def run_test (self):
        print "Mining blocks..."

        self.nodes[0].generate(1)
        do_not_shield_taddr = self.nodes[0].getnewaddress()

        self.nodes[0].generate(4)
        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 50)
        assert_equal(walletinfo['balance'], 0)
        self.sync_all()
        self.nodes[2].generate(1)
        self.nodes[2].getnewaddress()
        self.nodes[2].generate(1)
        self.nodes[2].getnewaddress()
        self.nodes[2].generate(1)
        self.sync_all()
        self.nodes[1].generate(101)
        self.sync_all()
        assert_equal(self.nodes[0].getbalance(), 50)
        assert_equal(self.nodes[1].getbalance(), 10)
        assert_equal(self.nodes[2].getbalance(), 30)

        # Prepare to send taddr->zaddr
        mytaddr = self.nodes[0].getnewaddress()
   