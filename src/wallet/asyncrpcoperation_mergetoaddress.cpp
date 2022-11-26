// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "asyncrpcoperation_mergetoaddress.h"

#include "amount.h"
#include "asyncrpcqueue.h"
#include "core_io.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "miner.h"
#include "net.h"
#include "netbase.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "script/interpreter.h"
#include "sodium.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "utiltime.h"
#include "wallet.h"
#include "walletdb.h"
#include "vidulum/IncrementalMerkleTree.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "paymentdisclosuredb.h"

using namespace libzcash;

extern UniValue sendrawtransaction(const UniValue& params, bool fHelp);

int mta_find_output(UniValue obj, int n)
{
    UniValue outputMapValue = find_value(obj, "outputmap");
    if (!outputMapValue.isArray()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Missing outputmap for JoinSplit operation");
    }

    UniValue outputMap = outputMapValue.get_array();
    assert(outputMap.size() == ZC_NUM_JS_OUTPUTS);
    for (size_t i = 0; i < outputMap.size(); i++) {
        if (outputMap[i].get_int() == n) {
            return i;
        }
    }

    throw std::logic_error("n is not present in outputmap");
}

AsyncRPCOperation_mergetoaddress::AsyncRPCOperation_mergetoaddress(
    boost::optional<TransactionBuilder> builder,
    CMutableTransaction contextualTx,
    std::vector<MergeToAddressInputUTXO> utxoInputs,
    std::vector<MergeToAddressInputSproutNote> sproutNoteInputs,
    std::vector<MergeToAddressInputSaplingNote> saplingNoteInputs,
    MergeToAddressRecipient recipient,
    CAmount fee,
    UniValue contextInfo) :
    tx_(contextualTx), utxoInputs_(utxoInputs), sproutNoteInputs_(sproutNoteInputs),
    saplingNoteInputs_(saplingNoteInputs), recipient_(recipient), fee_(fee), contextinfo_(contextInfo)
{
    if (fee < 0 || fee > MAX_MONEY) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Fee is out of range");
    }

    if (utxoInputs.empty() && sproutNoteInputs.empty() && saplingNoteInputs.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "No inputs");
    }

    if (std::get<0>(recipient).size() == 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Recipient parameter missing");
    }

    if (sproutNoteInputs.size() > 0 && saplingNoteInputs.size() > 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot send from both Sprout and Sapling addresses using z_mergetoaddress");
    }

    if (sproutNoteInputs.size() > 0 && builder) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Sprout notes are not supported by the TransactionBuilder");
    }

    isUsingBuilder_ = false;
    if (builder) {
        isUsingBuilder_ = true;
        builder_ = builder.get();
    }

    toTaddr_ = DecodeDestination(std::get<0>(recipient));
    isToTaddr_ = IsValidDestination(toTaddr_);
    isToZaddr_ = false;

    if (!isToTaddr_) {
        auto address = DecodePaymentAddress(std::get<0>(recipient));
        if (IsValidPaymentAddress(address)) {
            isToZaddr_ = true;
            toPaymentAddress_ = address;
        } else {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid recipient address");
        }
    }

    // Log the context info i.e. the call parameters to z_mergetoaddress
    if (LogAcceptCategory("zrpcunsafe")) {
        LogPrint("zrpcunsafe", "%s: z_mergetoaddress initialized (params=%s)\n", getId(), contextInfo.write());
    } else {
        LogPrint("zrpc", "%s: z_mergetoaddress initialized\n", getId());
    }

    // Lock UTXOs
    lock_utxos();
    lock_notes();

    // Enable payment disclosure if requested
    paymentDisclosureMode = fExperimentalMode && GetBoolArg("-paymentdisclosure", false);
}

AsyncRPCOperation_mergetoaddress::~AsyncRPCOperation_mergetoaddress()
{
}

void AsyncRPCOperation_mergetoaddress::main()
{
    if (isCancelled()) {
        unlock_utxos(); // clean up
        unlock_notes();
        return;
    }

    set_state(OperationStatus::EXECUTING);
    start_execution_clock();

    bool success = false;

#ifdef ENABLE_MINING
#ifdef ENABLE_WALLET
    GenerateBitcoins(false, NULL, 0);
#else
    GenerateBitcoins(false, 0);
#endif
#endif

    try {
        success = main_impl();
    } catch (const UniValue& objError) {
        int code = find_value(objError, "code").get_int();
        std::string message = find_value(objError, "message").get_str();
        set_error_code(code);
        set_error_message(message);
    } catch (const runtime_error& e) {
        set_error_code(-1);
        set_error_message("runtime error: " + string(e.what()));
    } catch (const logic_error& e) {
        set_error_code(-1);
        set_error_message("logic error: " + string(e.what()));
    } catch (const exception& e) {
        set_error_code(-1);
        set_error_message("general exception: " + string(e.what()));
    } catch (...) {
        set_error_code(-2);
        set_error_message("unknown error");
    }

#ifdef ENABLE_MINING
#ifdef ENABLE_WALLET
    GenerateBitcoins(GetBoolArg("-gen", false), pwalletMain, GetArg("-genproclimit", 1));
#else
    GenerateBitcoins(GetBoolArg("-gen", false), GetArg("-genproclimit", 1));
#endif
#endif

    stop_execution_clock();

    if (success) {
        set_state(OperationStatus::SUCCESS);
    } else {
        set_state(OperationStatus::FAILED);
    }

    std::string s = strprintf("%s: z_mergetoaddress finished (status=%s", getId(), getStateAsString());
    if (success) {
        s += strprintf(", txid=%s)\n", tx_.GetHash().ToString());
    } else {
        s += strprintf(", error=%s)\n", getErrorMessage());
    }
    LogPrintf("%s", s);

    unlock_utxos(); // clean up
    unlock_notes(); // clean up

    // !!! Payment disclosure START
    if (success && paymentDisclosureMode && paymentDisclosureData_.size() > 0) {
        uint256 txidhash = tx_.GetHash();
        std::shared_ptr<PaymentDisclosureDB> db = PaymentDisclosureDB::sharedInstance();
        for (PaymentDisclosureKeyInfo p : paymentDisclosureData_) {
            p.first.hash = txidhash;
            if (!db->Put(p.first, p.second)) {
                LogPrint("paymentdisclosure", "%s: Payment Disclosure: Error writing entry to database for key %s\n", getId(), p.first.ToString());
            } else {
                LogPrint("paymentdisclosure", "%s: Payment Disclosure: Successfully added entry to database for key %s\n", getId(), p.first.ToString());
            }
        }
    }
    // !!! Payment disclosure END
}

// Notes:
// 1. #1359 Currently there is no limit set on the number of joinsplits, so size of tx could be invalid.
// 2. #1277 Spendable notes are not locked, so an operation running in parallel could also try to use them.
bool AsyncRPCOperation_mergetoaddress::main_impl()
{
    assert(isToTaddr_ != isToZaddr_);

    bool isPureTaddrOnlyTx = (sproutNoteInputs_.empty() && saplingNoteInputs_.empty() && isToTaddr_);
    CAmount minersFee = fee_;

    size_t numInputs = utxoInputs_.size();

    // Check mempooltxinputlimit to avoid creating a transaction which the local mempool rejects
    size_t limit = (size_t)GetArg("-mempooltxinputlimit", 0);
    {
        LOCK(cs_main);
        if (NetworkUpgradeActive(chainActive.Height() + 1, Params().GetConsensus(), Consensus::UPGRADE_OVERWINTER)) {
            limit = 0;
        }
    }
    if (limit > 0 && numInputs > limit) {
        throw JSONRPCError(RPC_WALLET_ERROR,
                           strprintf("Number of transparent inputs %d is greater than mempooltxinputlimit of %d",
                                     numInputs, limit));
    }

    CAmount t_inputs_total = 0;
    for (MergeToAddressInputUTXO& t : utxoInputs_) {
        t_inputs_total += std::get<1>(t);
    }

    CAmount z_inputs_total = 0;
    for (const MergeToAddressInputSproutNote& t : sproutNoteInputs_) {
        z_inputs_total += std::get<2>(t);
    }

    for (const MergeToAddressInputSaplingNote& t : saplingNoteInputs_) {
        z_inputs_total += std::get<2>(t);
    }

    CAmount targetAmount = z_inputs_total + t_inputs_total;

    if (targetAmount <= minersFee) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS,
                           strprintf("Insufficient funds, have %s and miners fee is %s",
                                     FormatMoney(targetAmount), FormatMoney(minersFee)));
    }

    CAmount sendAmount = targetAmount - minersFee;

    // update the transaction with the UTXO inputs and output (if any)
    if (!isUsingBuilder_) {
        CMutableTransaction rawTx(tx_);
        for (const MergeToAddressInputUTXO& t : utxoInputs_) {
            CTxIn in(std::get<0>(t));
            rawTx.vin.push_back(in);
        }
        if (isToTaddr_) {
            CScript scriptPubKey = GetScriptForDestination(toTaddr_);
            CTxOut out(sendAmount, scriptPubKey);
            rawTx.vout.push_back(out);
        }
        tx_ = CTransaction(rawTx);
    }

    LogPrint(isPureTaddrOnlyTx ? "zrpc" : "zrpcunsafe", "%s: spending %s to send %s with fee %s\n",
             getId(), FormatMoney(targetAmount), FormatMoney(sendAmount), FormatMoney(minersFee));
    LogPrint("zrpc", "%s: transparent input: %s\n", getId(), FormatMoney(t_inputs_total));
    LogPrint("zrpcunsafe", "%s: private input: %s\n", getId(), FormatMoney(z_inputs_total));
    if (isToTaddr_) {
        LogPrint("zrpc", "%s: transparent output: %s\n", getId(), FormatMoney(sendAmount));
    } else {
        LogPrint("zrpcunsafe", "%s: private output: %s\n", getId(), FormatMoney(sendAmount));
    }
    LogPrint("zrpc", "%s: fee: %s\n", getId(), FormatMoney(minersFee));

    // Grab the current consensus branch ID
    {
        LOCK(cs_main);
        consensusBranchId_ = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());
    }

    /**
     * SCENARIO #0
     *
     * Sprout not involved, so we just use the TransactionBuilder and we're done.
     * 
     * This is based on code from AsyncRPCOperation_sendmany::main_impl() and should be refactored.
     */
    if (isUsingBuilder_) {
        builder_.SetFee(minersFee);


        for (const MergeToAddressInputUTXO& t : utxoInputs_) {
            COutPoint outPoint = std::get<0>(t);
            CAmount amount = std::get<1>(t);
            CScript scriptPubKey = std::get<2>(t);
            builder_.AddTransparentInput(outPoint, scriptPubKey, amount);
        }

        boost::optional<uint256> ovk;
        // Select Sapling notes
        std::vector<SaplingOutPoint> saplingOPs;
        std::vector<SaplingNote> saplingNotes;
        std::vector<SaplingExpandedSpendingKey> expsks;
        for (const MergeToAddressInputSaplingNote& saplingNoteInput: saplingNoteInputs_) {
            saplingOPs.push_back(std::get<0>(saplingNoteInput));
            saplingNotes.push_back(std::get<1>(saplingNoteInput));
            auto expsk = std::get<3>(saplingNoteInput);
            expsks.push_back(expsk);
            if (!ovk) {
                ovk = expsk.full_viewing_key().ovk;
            }
        }

        // Fetch Sapling anchor and witnesses
        uint256 anchor;
        std::vector<boost::optional<SaplingWitness>> witnesses;
        {
            LOCK2(cs_main, pwalletMain->cs_wallet);
            pwalletMain->GetSaplingNoteWitnesses(saplingOPs, witnesses, anchor);
        }

        // Add Sapling spends
        for (size_t i = 0; i < saplingNotes.size(); i++) {
            if (!witnesses[i]) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Missing witness for Sapling note");
            }
            assert(builder_.AddSaplingSpend(expsks[i], saplingNotes[i], anchor, witnesses[i].get()));
        }

        if (isToTaddr_) {
            if (!builder_.AddTransparentOutput(toTaddr_, sendAmount)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid output address, not a valid taddr.");
            }
        } else {
            std::string zaddr = std::get<0>(recipient_);
            std::string memo = std::get<1>(recipient_);
            std::array<unsigned char, ZC_MEMO_SIZE> hexMemo = get_memo_from_hex_string(memo);
            auto saplingPaymentAddress = boost::get<libzcash::SaplingPaymentAddress>(&toPaymentAddress_);
            if (saplingPaymentAddress == nullptr) {
                // This should never happen as we have already determined that the payment is to sapling
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Could not get Sapling payment address.");
            }
            if (saplingNoteInputs_.size() == 0 && utxoInputs_.size() > 0) {
                // Sending from t-addresses, which we don't have ovks for. Instead,
                // generate a common one from the HD seed. This ensures the data is
                // recoverable, while keeping it logically separate from the ZIP 32
                // Sapling key hierarchy, which the user might not be using.
                HDSeed seed;
                if (!pwalletMain->GetHDSeed(seed)) {
                    throw JSONRPCError(
                        RPC_WALLET_ERROR,
                        "AsyncRPCOperation_sendmany: HD seed not found");
                }
                ovk = ovkForShieldingFromTaddr(seed);
            }
            if (!ovk) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Sending to a Sapling address requires an ovk.");
            }
            builder_.AddSaplingOutput(ovk.get(), *saplingPaymentAddress, sendAmount, hexMemo);
        }


        // Build the transaction
        auto maybe_tx = builder_.Build();
        if (!maybe_tx) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Failed to build transaction.");
        }
        tx_ = maybe_tx.get();

        // Send the transaction
        // TODO: Use CWallet::CommitTransaction instead of sendrawtransaction
        auto signedtxn = EncodeHexTx(tx_);
        if (!testmode) {
            UniValue params = UniValue(UniValue::VARR);
            params.push_back(signedtxn);
            UniValue sendResultValue = sendrawtransaction(params, false);
            if (sendResultValue.isNull()) {
                throw JSONRPCError(RPC_WALLET_ERROR, "sendrawtransaction did not return an error or a txid.");
            }

            auto txid = sendResultValue.get_str();

            UniValue o(UniValue::VOBJ);
            o.push_back(Pair("txid", txid));
            set_result(o);
        } else {
            // Test mode does not send the transaction to the network.
            UniValue o(UniValue::VOBJ);
            o.push_back(Pair("test", 1));
            o.push_back(Pair("txid", tx_.GetHash().ToString()));
            o.push_back(Pair("hex", signedtxn));
            set_result(o);
        }

        return true;
    }
    /**
     * END SCENARIO #0
     */


    /**
     * SCENARIO #1
     *
     * taddrs -> taddr
     *
     * There are no zaddrs or joinsplits involved.
     */
    if (isPureTaddrOnlyTx) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("rawtxn", EncodeHexTx(tx_)));
        sign_send_raw_transaction(obj);
        return true;
    }
    /**
     * END SCENARIO #1
     */


    // Prepare raw transaction to handle JoinSplits
    CMutableTransaction mtx(tx_);
    crypto_sign_keypair(joinSplitPubKey_.begin(), joinSplitPrivKey_);
    mtx.joinSplitPubKey = joinSplitPubKey_;
    tx_ = CTransaction(mtx);
    std::string hexMemo = std::get<1>(recipient_);


    /**
     * SCENARIO #2
     *
     * taddrs -> zaddr
     *
     * We only need a single JoinSplit.
     */
    if (sproutNoteInputs_.empty() && isToZaddr_) {
        // Create JoinSplit to target z-addr.
        MergeToAddressJSInfo info;
        info.vpub_old = sendAmount;
        info.vpub_new = 0;

        JSOutput jso = JSOutput(boost::get<libzcash::SproutPaymentAddress>(toPaymentAddress_), sendAmount);
        if (hexMemo.size() > 0) {
            jso.memo = get_memo_from_hex_string(hexMemo);
        }
        info.vjsout.push_back(jso);

        UniValue obj(UniValue::VOBJ);
        obj = perform_joinsplit(info);
        sign_send_raw_transaction(obj);
        return true;
    }
    /**
     * END SCENARIO #2
     */


    // Copy zinputs to more flexible containers
    std::deque<MergeToAddressInputSproutNote> zInputsDeque;
    for (const auto& o : sproutNoteInputs_) {
        zInputsDeque.push_back(o);
    }

    // When spending notes, take a snapshot of note witnesses and anchors as the treestate will
    // change upon arrival of new blocks which contain joinsplit transactions.  This is likely
    // to happen as creating a chained joinsplit transaction can take longer than the block interval.
    {
        LOCK2(cs_main, pwalletMain->cs_wallet);
        for (auto t : sproutNoteInputs_) {
            JSOutPoint jso = std::get<0>(t);
            std::vector<JSOutPoint> vOutPoints = {jso};
            uint256 inputAnchor;
            std::vector<boost::optional<SproutWitness>> vInputWitnesses;
            pwalletMain->GetSproutNoteWitnesses(vOutPoints, vInputWitnesses, inputAnchor);
            jsopWitnessAnchorMap[jso.ToString()] = MergeToAddressWitnessAnchorData{vInputWitnesses[0], inputAnchor};
        }
    }

    /**
     * SCENARIO #3
     *
     * zaddrs -> zaddr
     * taddrs ->
     *
     * zaddrs ->
     * taddrs -> taddr
     *
     * Send to zaddr by chaining JoinSplits together and immediately consuming any change
     * Send to taddr by creating dummy z outputs and accumulating value in a change note
     * which is used to set vpub_new in the last chained joinsplit.
     */
    UniValue obj(UniValue::VOBJ);
    CAmount jsChange = 0;          // this is updated after each joinsplit
    int changeOutputIndex = -1;    // this is updated after each joinsplit if jsChange > 0
    bool vpubOldProcessed = false; // updated when vpub_old for taddr inputs is set in first joinsplit
    bool vpubNewProcessed = false; // updated when vpub_new for miner fee and taddr outputs is set in last joinsplit

    // At this point, we are guaranteed to have at least one input note.
    // Use address of first input note as the temporary change address.
    SproutSpendingKey changeKey = std::get<3>(zInputsDeque.front());
    SproutPaymentAddress changeAddress = changeKey.address();

    CAmount vpubOldTarget = 0;
    CAmount vpubNewTarget = 0;
    if (isToTaddr_) {
        vpubNewTarget = z_inputs_total;
    } else {
        if (utxoInputs_.empty()) {
            vpubNewTarget = minersFee;
        } else {
            vpubOldTarget = t_inputs_total - minersFee;
        }
    }

    // Keep track of treestate within this transaction
    boost::unordered_map<uint256, SproutMerkleTree, CCoinsKeyHasher> intermediates;
    std::vector<uint256> previousCommitments;

    while (!vpubNewProcessed) {
        MergeToAddressJSInfo info;
        info.vpub_old = 0;
        info.vpub_new = 0;

        // Set vpub_old in the first joinsplit
        if (!vpubOldProcessed) {
            if (t_inputs_total < vpubOldTarget) {
                throw JSONRPCError(RPC_WALLET_ERROR,
                                   strprintf("Insufficient transparent funds for vpub_old %s (miners fee %s, taddr inputs %s)",
                                             FormatMoney(vpubOldTarget), FormatMoney(minersFee), FormatMoney(t_inputs_total)));
            }
            info.vpub_old += vpubOldTarget; // funds flowing from public pool
            vpubOldProcessed = true;
        }

        CAmount jsInputValue = 0;
        uint256 jsAnchor;
        std::vector<boost::optional<SproutWitness>> witnesses;

        JSDescription prevJoinSplit;

        // Keep track of previous JoinSplit and its commitments
        if (tx_.vjoinsplit.size() > 0) {
            prevJoinSplit = tx_.vjoinsplit.back();
        }

        // If there is no change, the chain has terminated so we can reset the tracked treestate.
        if (jsChange == 0 && tx_.vjoinsplit.size() > 0) {
            intermediates.clear();
            previousCommitments.clear();
        }

        //
        // Consume change as the first input of the JoinSplit.
        //
        if (jsChange > 0) {
            LOCK2(cs_main, pwalletMain->cs_wallet);

            // Update tree state with previous joinsplit
            SproutMerkleTree tree;
            auto it = intermediates.find(prevJoinSplit.anchor);
            if (it != intermediates.end()) {
                tree = it->second;
            } else if (!pcoinsTip->GetSproutAnchorAt(prevJoinSplit.anchor, tree)) {
                throw JSONRPC