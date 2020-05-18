#!/usr/bin/env python2
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test REST interface
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, \
    initialize_chain_clean, start_nodes, connect_nodes_bi

import struct
import binascii
import json
import StringIO
from decimal import Decimal

try:
    import http.client as httplib
except ImportError:
    import httplib
try:
    import urllib.parse as urlparse
except ImportError:
    import urlparse

def deser_uint256(f):
    r = 0
    for i in range(8):
        t = struct.unpack(b"<I", f.read(4))[0]
        r += t << (i * 32)
    return r

# allows simple http get calls
def http_get_call(host, port, path, response_object = 0):
    conn = httplib.HTTPConnection(host, port)
    conn.request('GET', path)

    if response_object:
        return conn.getresponse()

    return conn.getresponse().read()

# allows simple http post calls with a request body
def http_post_call(host, port, path, requestdata = '', response_object = 0):
    conn = httplib.HTTPConnection(host, port)
    conn.request('POST', path, requestdata)

    if response_object:
        return conn.getresponse()

    return conn.getresponse().read()

class RESTTest (BitcoinTestFramework):
    FORMAT_SEPARATOR = "."

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 3)

    def setup_network(self, split=False):
        self.nodes = start_nodes(3, self.options.tmpdir)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.is_network_split=False
        self.sync_all()

    def run_test(self):
        url = urlparse.urlparse(self.nodes[0].url)
        print "Mining blocks..."

        self.nodes[0].generate(1)
        self.sync_all()
        self.nodes[2].generate(100)
        self.sync_all()

        assert_equal(self.nodes[0].getbalance(), 10)

        txid = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 0.1)
        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()
        bb_hash = self.nodes[0].getbestblockhash()

        assert_equal(self.nodes[1].getbalance(), Decimal("0.1")) # balance now should be 0.1 on node 1

        # load the latest 0.1 tx over the REST API
        json_string = http_get_call(url.hostname, url.port, '/rest/tx/'+txid+self.FORMAT_SEPARATOR+"json")
        json_obj = json.loads(json_string)
        vintx = json_obj['vin'][0]['txid'] # get the vin to later check for utxo (should be spent by then)
        # get n of 0.1 outpoint
        n = 0
        for vout in json_obj['vout']:
            if vout['value'] == 0.1:
                n = vout['n']


        ######################################
        # GETUTXOS: query a unspent outpoint #
        ######################################
        json_request = '/checkmempool/'+txid+'-'+str(n)
        json_string = http_get_call(url.hostname, url.port, '/rest/getutxos'+json_request+self.FORMAT_SEPARATOR+'json')
        json_obj = json.loads(json_string)

        #check chainTip response
        assert_equal(json_obj['chaintipHash'], bb_hash)

        #make sure there is one utxo
        assert_equal(len(json_obj['utxos']), 1)
        assert_equal(json_obj['utxos'][0]['value'], 0.1)


        ################################################
        # GETUTXOS: now query a already spent outpoint #
        ################################################
        json_request = '/checkmempool/'+vintx+'-0'
        json_string = http_get_call(url.hostname, url.port, '/rest/getutxos'+json_request+self.FORMAT_SEPARATOR+'json')
        json_obj = json.loads(json_string)

        # check chainTip response
        assert_equal(json_obj['chaintipHash'], bb_hash)

        # make sure there is no utox in the response because this oupoint has been spent
        assert_equal(len(json_obj['utxos']), 0)

        # check bitmap
        assert_equal(json_obj['bitmap'], "0")


        ##################################################
        # GETUTXOS: now check both wit