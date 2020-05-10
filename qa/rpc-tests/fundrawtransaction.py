#!/usr/bin/env python2
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_greater_than, \
    initialize_chain_clean, start_nodes, connect_nodes_bi, stop_nodes, \
    wait_bitcoinds

from decimal import Decimal

# Create one-input, one-output, no-fee transaction:
class RawTransactionsTest(BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 3)

    def setup_network(self, split=False):
        self.nodes = start_nodes(3, self.options.tmpdir,
                           extra_args=[['-experimentalfeatures', '-developerencryptwallet']] * 4)

        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)

        self.is_network_split=False
        self.sync_all()

    def run_test(self):
        print "Mining blocks..."
        feeTolerance = Decimal(0.00000002) #if the fee's positive delta is higher than this value tests will fail, neg. delta always fail the tests

        self.nodes[2].generate(1)
        self.sync_all()
        self.nodes[0].generate(101)
        self.sync_all()
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(),1.5);
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(),1.0);
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(),5.0);
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        ###############
        # simple test #
        ###############
        inputs  = [ ]
        outputs = { self.nodes[0].getnewaddress() : 1.0 }
        rawtx   = self.nodes[2].createrawtransaction(inputs, outputs)
        dec_tx  = self.nodes[2].decoderawtransaction(rawtx)
        rawtxfund = self.nodes[2].fundrawtransaction(rawtx)
        fee = rawtxfund['fee']
        dec_tx  = self.nodes[2].decoderawtransaction(rawtxfund['hex'])
        assert_equal(len(dec_tx['vin']) > 0, True) #test if we have enought inputs

        ##############################
        # simple test with two coins #
        ##############################
        inputs  = [ ]
        outputs = { self.nodes[0].getnewaddress() : 2.2 }
        rawtx   = self.nodes[2].createrawtransaction(inputs, outputs)
        dec_tx  = self.nodes[2].decoderawtransaction(rawtx)

        rawtxfund = self.nodes[2].fundrawtransaction(rawtx)
        fee = rawtxfund['fee']
        dec_tx  = self.nodes[2].decoderawtransaction(rawtxfund['hex'])
        assert_equal(len(dec_tx['vin']) > 0, True) #test if we have enough inputs

        ##############################
        # simple test with two coins #
        ##############################
        inputs  = [ ]
        outputs = { self.nodes[0].getnewaddress() : 2.6 }
        rawtx   = self.nodes[2].createrawtransaction(inputs, outputs)
        dec_tx  = self.nodes[2].decoderawtransaction(rawtx)

        rawtxfund = self.nodes[2].fundrawtransaction(rawtx)
        fee = rawtxfund['fee']
        dec_tx  = self.nodes[2].decoderawtransaction(rawtxfund['hex'])
        assert_equal(len(dec_tx['vin']) > 0, True)
        assert_equal(dec_tx['vin'][0]['scriptSig']['hex'], '')


        ################################
        # simple test with two outputs #
        ################################
        inputs  = [ ]
        outputs = { self.nodes[0].getnewaddress() : 2.6, self.nodes[1].getnewaddress() : 2.5 }
        rawtx   = self.nodes[2].createrawtransaction(inputs, outputs)
        dec_tx  = self.nodes[2].decoderawtransaction(rawtx)

        rawtxfund = self.nodes[2].fundrawtransaction(rawtx)
        fee = rawtxfund['fee']
        dec_tx  = self.nodes[2].decoderawtransaction(rawtxfund['hex'])
        totalOut = 0
        for out in dec_tx['vout']:
            totalOut += out['value']

        assert_equal(len(dec_tx['vin']) > 0, True)
        assert_equal(dec_tx['vin'][0]['scriptSig']['hex'], '')


        #########################################################################
        # test a fundrawtransaction with a VIN greater than the required amount #
        #########################################################################
        utx = False
        listunspent = self.nodes[2].listunspent()
        for aUtx in listunspent:
            if aUtx['amount'] == 5.0:
                utx = aUtx
                break;

        assert_equal(utx!=False, True)

        inputs  = [ {'txid' : utx['txid'], 'vout' : utx['vout']}]
        outputs = { self.nodes[0].getnewaddress() : 1.0 }
        rawtx   = self.nodes[2].createrawtransaction(inputs, outputs)
        dec_tx  = self.nodes[2].decoderawtransaction(rawtx)
        assert_equal(utx['txid'], dec_tx['vin'][0]['txid'])

        rawtxfund = self.nodes[2].fundrawtransaction(rawtx)
        fee = rawtxfund['fee']
        dec_tx  = self.nodes[2].decoderawtransaction(rawtxfund['hex'])
        totalOut = 0
        for out in dec_tx['vout']:
            totalOut += out['value']

        assert_equal(fee + totalOut, utx['amount']) #compare vin total and totalout+fee



        #####################################################################
        # test a fundrawtransaction with which will not get a change output #
        #####################################################################
        utx = False
        listunspent = self.nodes[2].listunspent()
        for aUtx in listunspent:
            if aUtx['amount'] == 5.0:
                utx = aUtx
                break;

        assert_equal(utx!=False, True)

        inputs  = [ {'txid' : utx['txid'], 'vout' : utx['vout']}]
        outputs = { self.nodes[0].getnewaddress() : Decimal(5.0) - fee - feeTolerance }
        rawtx   = self.nodes[2].createrawtransaction(inputs, outputs)
        dec_tx  = self.nodes[2].decoderawtransaction(rawtx)
        assert_equal(utx['txid'], dec_tx['vin'][0]['txid'])

        rawtxfund = self.nodes[2].fundrawtransaction(rawtx)
        fee = rawtxfund['fee']
        dec_tx  = self.nodes[2].decoderawtransaction(rawtxfund['hex'])
        totalOut = 0
        for out in dec_tx['vout']:
            totalOut += out['value']

        assert_equal(rawtxfund['changepos'], -1)
        assert_equal(fee + totalOut, utx['amount']) #compare vin total and totalout+fee



        #########################################################################
        # test a fundrawtransaction with a VIN smaller than the required amount #
        #########################################################################
        utx = False
        listunspent = self.nodes[2].listunspent()
        for aUtx in listunspent:
            if aUtx['amount'] == 1.0:
                utx = aUtx
                break;

        assert_equal(utx!=False, True)

        inputs  = [ {'txid' : utx['txid'], 'vout' : utx['vout']}]
        outputs = { self.nodes[0].getnewaddress() : 1.0 }
        rawtx   = self.nodes[2].createrawtransaction(inputs, outputs)

        # 4-byte version + 1-byte vin count + 36-byte prevout then script_len
        rawtx = rawtx[:82] + "0100" + rawtx[84:]

        dec_tx  = self.nodes[2].decoderawtransaction(rawtx)
        assert_equal(utx['txid'], dec_tx['vin'][0]['txid'])
        assert_equal("00", dec_tx['vin'][0]['scriptSig']['hex'])

        rawtxfund = self.nodes[2].fundrawtransaction(rawtx)
        fee = rawtxfund['fee']
        dec_tx  = self.nodes[2].decoderawtransaction(rawtxfund['hex'])
        totalOut = 0
        matchingOuts = 0
        for i, out in enumerate(dec_tx['vout']):
            totalOut += out['value']
            if outputs.has_key(out['scriptPubKey']['addresses'][0]):
                matchingOuts+=1
            else:
                assert_equal(i, rawtxfund['changepos'])

        assert_equal(utx['txid'], dec_tx['vin'][0]['txid'])
        assert_equal("00", dec_tx['vin'][0]['scriptSig']['hex'])

        assert_equal(matchingOuts, 1)
        assert_equal(len(dec_tx['vout']), 2)


        ###########################################
        # test a fundrawtransaction with two VINs #
        ###########################################
        utx  = False
        utx2 = False
        listunspent = self.nodes[2].listunspent()
        for aUtx in listunspent:
            if aUtx['amount'] == 1.0:
                utx = aUtx
            if aUtx['amount'] == 5.0:
                utx2 = aUtx


        assert_equal(utx!=False, True)

        inputs  = [ {'txid' : utx['txid'], 'vout' : utx['vout']},{'txid' : utx2['txid'], 'vout' : utx2['vout']} ]
        outputs = { self.nodes[0].getnewaddress() : 6.0 }
        rawtx   = self.nodes[2].createrawtransaction(inputs, outputs)
        dec_tx  = self.nodes[2].decoderawtransaction(rawtx)
        assert_equal(utx['txid'], dec_tx['vin'][0]['txid'])

        rawtxfund = self.nodes[2].fundrawtransaction(rawtx)
        fee = rawtxfund['fee']
        dec_tx  = self.nodes[2].decoderawtransaction(rawtxfund['hex'])
        totalOut = 0
        matchingOuts = 0
        for out in dec_tx['vout']:
            totalOut += out['value']
            if outputs.has_key(out['scriptPubKey']['addresses'][0]):
                matchingOuts+=1

        assert_equal(matchingOuts, 1)
        assert_equal(len(dec_tx['vout']), 2)

        matchingIns = 0
        for vinOut in dec_tx['vin']:
            for vinIn in inputs:
                if vinIn['txid'] == vinOut['txid']:
                    matchingIns+=1

        assert_equal(matchingIns, 2) #we now must see two vins identical to vins given as params

        #########################################################
        # test a fundrawtransaction with two VINs and two vOUTs #
        #########################################################
        utx  = False
        utx2 = False
        listunspent = self.nodes[2].listunspent()
        for aUtx in listunspent:
            if aUtx['amount'] == 1.0:
                utx = aUtx
            if aUtx['amount'] == 5.0:
                utx2 = aUtx


        assert_equal(utx!=False, True)

        inputs  = [ {'txid' : utx['txid'], 'vout' : utx['vout']},{'txid' : utx2['txid'], 'vout' : utx2['vout']} ]
        outputs = { self.nodes[0].getnewaddress() : 6.0, self.nodes[0].getnewaddress() : 1.0 }
        rawtx   = sel