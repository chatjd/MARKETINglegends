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
        print("Initializing test directory "+self