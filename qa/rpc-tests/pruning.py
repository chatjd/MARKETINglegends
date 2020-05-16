
#!/usr/bin/env python2
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test pruning code
# ********
# WARNING:
# This test uses 4GB of disk space.
# This test takes 30 mins or more (up to 2 hours)
# ********

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import initialize_chain_clean, start_node, \
    connect_nodes, stop_node, sync_blocks

import os.path
import time

def calc_usage(blockdir):
    return sum(os.path.getsize(blockdir+f) for f in os.listdir(blockdir) if os.path.isfile(blockdir+f))/(1024*1024)

class PruneTest(BitcoinTestFramework):

    def __init__(self):
        self.utxo = []
        self.address = ["",""]

        # Some pre-processing to create a bunch of OP_RETURN txouts to insert into transactions we create
        # So we have big transactions and full blocks to fill up our block files

        # create one script_pubkey
        script_pubkey = "6a4d0200" #OP_RETURN OP_PUSH2 512 bytes
        for i in xrange (512):
            script_pubkey = script_pubkey + "01"
        # concatenate 128 txouts of above script_pubkey which we'll insert before the txout for change
        self.txouts = "81"
        for k in xrange(128):
            # add txout value
            self.txouts = self.txouts + "0000000000000000"
            # add length of script_pubkey
            self.txouts = self.txouts + "fd0402"
            # add script_pubkey
            self.txouts = self.txouts + script_pubkey


    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 3)

    def setup_network(self):
        self.nodes = []
        self.is_network_split = False

        # Create nodes 0 and 1 to mine
        self.nodes.append(start_node(0, self.options.tmpdir, ["-debug","-maxreceivebuffer=20000","-blockmaxsize=999000", "-checkblocks=5"], timewait=900))
        self.nodes.append(start_node(1, self.options.tmpdir, ["-debug","-maxreceivebuffer=20000","-blockmaxsize=999000", "-checkblocks=5"], timewait=900))

        # Create node 2 to test pruning
        self.nodes.append(start_node(2, self.options.tmpdir, ["-debug","-maxreceivebuffer=20000","-prune=550"], timewait=900))