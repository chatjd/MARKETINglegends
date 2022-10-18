// Copyright (c) 2017 The Zcash developers
// Copyright (c) 2017-2018 The SnowGem developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
#include "test/test_bitcoin.h"
#include "torcontrol.cpp"

#include <boost/test/unit_test.hpp>


BOOST_FIXTURE_TEST_SUITE(torcontrol_tests, BasicTestingSetup)

void CheckSplitTorReplyLine(std::string input, std::string command, std::string args)
{
    BOOST_TEST_MESSAGE(std::string("CheckSplitTorReplyLine(") + input + ")");
    auto ret = SplitTorReplyLine(input);
    BOOST_CHECK_EQUAL(ret.first, command);
    BOOST_CHECK_EQUAL(ret.second, args);
}

BOOST_AUTO_TEST_CASE(util_SplitTorReplyLine)
{
    // Data we should receive during normal