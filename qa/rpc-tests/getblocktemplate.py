#!/usr/bin/env python2
# Copyright (c) 2016 The Zcash developers
# Copyright (c) 2017-2018 The SnowGem developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import initialize_chain_clean, start_nodes, \
    connect_nodes_bi


class GetBlockTemplateTest(BitcoinTestFramework):
    '''
    Test getblocktemplate.
    '''

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_network(self, split=False):
        self.nodes = start_nodes(2, self.options.tmpdir)
        connect_nodes_bi(self.nodes,0,1)
        