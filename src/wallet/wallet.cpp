// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/wallet.h"

#include "checkpoints.h"
#include "coincontrol.h"
#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "consensus/consensus.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "masternode-budget.h"
#include "net.h"
#include "rpc/protocol.h"
#include "script/script.h"
#include "script/sign.h"
#include "spork.h"
#include "swifttx.h"
#include "timedata.h"
#include "utilmoneystr.h"
#include "vidulum/Note.hpp"
#include "crypter.h"
#include "vidulum/zip32.h"

#include <assert.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

using namespace std;
using namespace libzcash;

/**
 * Settings
 */
CFeeRate payTxFee(DEFAULT_TRANSACTION_FEE);
CAmount maxTxFee = DEFAULT_TRANSACTION_MAXFEE;
unsigned int nTxConfirmTarget = DEFAULT_TX_CONFIRM_TARGET;
bool bSpendZeroConfChange = true;
bool fSendFreeTransactions = false;
bool fPayAtLeastCustomFee = true;

/**
 * Fees smaller than this (in satoshi) are considered zero fee (for transaction creation)
 * Override with -mintxfee
 */
CFeeRate CWallet::minTxFee = C