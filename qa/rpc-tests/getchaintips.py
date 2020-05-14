#!/usr/bin/env python2
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Exercise the getchaintips API.  We introduce a network split, work
# on chains of different lengths, and join the network together again.
# This gives us two tips, verify that it works.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

class GetChainTipsTest (BitcoinTestFramework):

    def run_test (self):
        BitcoinTestFramework.run_test (self)

        tips = self.nodes[0].getchaintips ()
        assert_equal (len (tips), 1)
        assert_equal (tips[0]['branchlen'], 0)
        assert_equal (tips[0]['height'], 200)
        assert_equal (tips[0]['status'], 'active')

        # Split the network and build two c