// Copyright (c) 2016 The Vidulum developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ASYNCRPCOPERATION_SENDMANY_H
#define ASYNCRPCOPERATION_SENDMANY_H

#include "asyncrpcoperation.h"
#include "amount.h"
#include "primitives/transaction.h"
#include "transaction_builder.h"
#include "vidulum/JoinSplit.hpp"
#include "vidulum/Address.hpp"
#include "wallet.h"
#include "paymentdisclosure.h"

#include <array>
#include <unordered_map>
#include <tuple>

#include <univalue.h>

// Default transaction fee if caller does not specify one.
#define ASYNC_RPC_OPERATION_DEFAULT_MINERS_FEE   10000

using namespace libzcash;

// A recipient is a tuple of address, amount, memo (optional if zaddr)
typedef std::tuple<std::string, CAmount, std::string> SendManyRecipient;

// Input UTXO is a tuple (quadruple) of txid, vout, amount, coinbase)
typedef std::tuple<uint256, int, CAmount, bool> SendManyInputUTXO;

// Input JSOP is a tuple of JSOutpoint, note and amount
typedef std::tuple<JSOutPoint, SproutNote, CAmount> SendManyInputJSOP;

// Package of info which is passed to perform_joinsplit methods.
struct AsyncJoinSplitInfo
{
    std::vector<JSInput> vjsin;
    std::vector<JSOutput> vjsout;
    std::vector<SproutNote> notes;
    CAmount vpub_old = 0;
    CAmount vpub_new = 0;
};

// A struct to help us track the witness and anchor for a given JSOutPoint
struct WitnessAnchorData {
	boost::optional<SproutWitness> witness;
	uint256 anchor;
};

class AsyncRPCOperation_sendmany : public AsyncRPCOperation {
public:
    AsyncRPCOperation_sendmany(
        boost::optional<TransactionBuilder> builder,
        CMutableTransaction contextualTx,
        std::string fromAddress,
        std::vector<SendManyRecipient> tOutputs,
        std::vector<SendManyRecipient> zOutputs,
        int minDepth,
        CAmount fee = ASYNC_RPC_OPERATION_DEFAULT_MINERS_FEE,
        UniValue contextInfo = NullUniValue);
    virtual ~AsyncRPCOperation_sendmany();
    
    // We don't want to be copied or moved around
    AsyncRPCOperation_sendmany(AsyncRPCOperation_sendmany const&) = delete;             // Copy construct
    AsyncRPCOperation_sendmany(AsyncRPCOperation_sendmany&&) = delete;                  // Move construct
    AsyncRPCOperation_sendmany& operator=(AsyncRPCOperation_sendmany const&) = delete;  // Copy assign
    AsyncRPCOperation_sendmany& operator=(AsyncRPCOperation_sendmany &&) = delete;      // Move assign
    
    virtual void main();

    virtual UniValue getStatus() const;

    bool testmode = false;  // Set to true to disable sending txs and generating proofs

    bool paymentDisclosureMode = false; // Set to true to save esk for encrypted notes in payment disclosure database.

private:
    friend class TEST_FRIEND_AsyncRPCOperation_sendmany;    // class for unit testing

    UniValue contextinfo_;     // optional data to i