// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Bitcoin Test Suite

#include "test_bitcoin.h"

#include "crypto/common.h"

#include "key.h"
#include "main.h"
#include "random.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "rpc/server.h"
#include "rpc/register.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "wallet/db.h"
#include "wallet/wallet.h"
#endif

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>

#include "librustzcash.h"

CClientUIInterface uiInterface; // Declared but not defined in ui_interface.h
CWallet* pwalletMain;
ZCJoinSplit *pvidulumParams;

extern bool fPrintToConsole;
extern void noui_connect();

JoinSplitTestingSetup::JoinSplitTestingSetup()
{
    boost::filesystem::path pk_path = ZC_GetParamsDir() / "sprout-proving.key";
    boost::filesystem::path vk_