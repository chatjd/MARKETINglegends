#!/usr/bin/env python2
#
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#

from test_framework.test_framework import ComparisonTestFramework
from test_framework.util import assert_equal
from test_framework.comptool import TestManager, TestInstance
from test_framework.mininode import NetworkThread
from test_framework.blocktools import create_block, create_coinbase, create_transaction

import copy
import time


'''
In this test we connect to one node over p2p, and test block requests:
1) Valid blocks should be requested and become chain tip.
2) Invalid block with duplicated transaction should be re-requested.
3) Invalid block with bad coinbase value should be rejected and not
re-requested.
'''

# Use the ComparisonTestFramework with 1 node: only use --testbinary.
class InvalidBlockRequestTest(ComparisonTestFramework):

    ''' Can either run this test as 1 node with expected answers, or two and compare them.
        Change the "outcome" variable from each TestInstance object to only do the comparison. '''
    def __init__(self):
        self.num_nodes = 1

    def run_test(self):
        test = TestManager(self, self.options.tmpdir)
        test.add_all_connections(self.nodes)
        self.tip = None
        self.block_time = None
        NetworkThread().start() # Start up network handling in another thread
        test.run()

    def get_tests(self):
        if self.tip is None:
            self.tip = int ("0x" + self.nodes[0].getbestblockhash() + "L", 0)
        self.bloc