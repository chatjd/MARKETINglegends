// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2017-2018 The SnowGem developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "main.h"

#include "addrman.h"
#include "amount.h"
#include "masternode-budget.h"
#include "masternode-sync.h"
#include "masternode.h"
#include "masternodeman.h"
#include "obfuscation.h"
#include "util.h"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include "key_io.h"

CBudgetManager budget;
CCriticalSection cs_budget;

std::map<uint256, int64_t> askedForSourceProposalOrBudget;
std::vector<CBudgetProposalBroadcast> vecImmatureBudgetProposals;
std::vector<CFinalizedBudgetBroadcast> vecImmatureFinalizedBudgets;

int nSubmittedFinalBudget;

int GetBudgetPaymentCycleBlocks()
{
    // Amount of blocks in a months period of time (using 1 minutes per block) = (60*24*30)
    if (NetworkIdFromCommandLine() == CBaseChainParams::MAIN) return (60*24*30); //1 month
    //for testing purposes

    return 144; //ten times per day
}

bool IsBudgetCollateralValid(uint256 nTxCollateralHash, uint256 nExpectedHash, std::string& strError, int64_t& nTime, int& nConf)
{
    CTransaction txCollateral;
    uint256 nBlockHash;
    if (!GetTransaction(nTxCollateralHash, txCollateral, nBlockHash, true)) {
        strError = strprintf("Can't find collateral tx %s", txCollateral.ToString());
        LogPrint("masternode","CBudgetProposalBroadcast::IsBudgetCollateralValid - %s\n", strError);
        return false;
    }

    if (txCollateral.vout.size() < 1) return false;
    if (txCollateral.nLockTime != 0) return false;

    CScript findScript;
    findScript << OP_RETURN << ToByteVector(nExpectedHash);

    bool foundOpReturn = false;
    BOOST_FOREACH (const CTxOut o, txCollateral.vout) {
        if (!o.scriptPubKey.IsNormalPaymentScript() && !o.scriptPubKey.IsUnspendable()) {
            strError = strprintf("Invalid Script %s", txCollateral.ToString());
            LogPrint("masternode","CBudgetProposalBroadcast::IsBudgetCollateralValid - %s\n", strError);
            return false;
        }
        if (o.scriptPubKey == findScript && o.nValue >= PROPOSAL_FEE_TX) foundOpReturn = true;
    }

    if (!foundOpReturn) {
        strError = strprintf("Couldn't find opReturn %s in %s", nExpectedHash.ToString(), txCollateral.ToString());
        LogPrint("masternode","CBudgetProposalBroadcast::IsBudgetCollateralValid - %s\n", strError);
        return false;
    }

    // RETRIEVE CONFIRMATIONS AND NTIME
    /*
        - nTime starts as zero and is passed-by-reference out of this function and stored in the external proposal
        - nTime is never validated via the hashing mechanism and comes from a full-validated source (the blockchain)
    */

    int conf = GetIXConfirmations(nTxCollateralHash);
    if (nBlockHash != uint256()) {
        BlockMap::iterator mi = mapBlockIndex.find(nBlockHash);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex* pindex = (*mi).second;
            if (chainActive.Contains(pindex)) {
                conf += chainActive.Height() - pindex->nHeight + 1;
                nTime = pindex->nTime;
            }
        }
    }

    nConf = conf;

    //if we're syncing we won't have swiftTX information, so accept 1 confirmation
    if (conf >= Params().Budget_Fee_Confirmations()) {
        return true;
    } else {
        strError = strprintf("Collateral requires at least %d confirmations - %d confirmations", Params().Budget_Fee_Confirmations(), conf);
        LogPrint("masternode","CBudgetProposalBroadcast::IsBudgetCollateralValid - %s - %d confirmations\n", strError, conf);
        return false;
    }
}

void CBudgetManager::CheckOrphanVotes()
{
    LOCK(cs);


    std::string strError = "";
    std::map<uint256, CBudgetVote>::iterator it1 = mapOrphanMasternodeBudgetVotes.begin();
    while (it1 != mapOrphanMasternodeBudgetVotes.end()) {
        if (budget.UpdateProposal(((*it1).second), NULL, strError)) {
            LogPrint("masternode","CBudgetManager::CheckOrphanVotes - Proposal/Budget is known, activating and removing orphan vote\n");
            mapOrphanMasternodeBudgetVotes.erase(it1++);
        } else {
            ++it1;
        }
    }
    std::map<uint256, CFinalizedBudgetVote>::iterator it2 = mapOrphanFinalizedBudgetVotes.begin();
    while (it2 != mapOrphanFinalizedBudgetVotes.end()) {
        if (budget.UpdateFinalizedBudget(((*it2).second), NULL, strError)) {
            LogPrint("masternode","CBudgetManager::CheckOrphanVotes - Proposal/Budget is known, activating and removing orphan vote\n");
            mapOrphanFinalizedBudgetVotes.erase(it2++);
        } else {
            ++it2;
        }
    }
    LogPrint("masternode","CBudgetManager::CheckOrphanVotes - Done\n");
}

void CBudgetManager::SubmitFinalBudget()
{
    static int nSubmittedHeight = 0; // height at which final budget was submitted last time
    int nCurrentHeight;

    {
        TRY_LOCK(cs_main, locked);
        if (!locked) return;
        if (!chainActive.Tip()) return;
        nCurrentHeight = chainActive.Height();
    }

    int nBlockStart = nCurrentHeight - nCurrentHeight % GetBudgetPaymentCycleBlocks() + GetBudgetPaymentCycleBlocks();
    if (nSubmittedHeight >= nBlockStart){
        LogPrint("masternode","CBudgetManager::SubmitFinalBudget - nSubmittedHeight(=%ld) < nBlockStart(=%ld) condition not fulfilled.\n", nSubmittedHeight, nBlockStart);
        return;
    }
    // Submit final budget during the last 2 days before payment for Mainnet, about 9 minutes for Testnet
    int nFinalizationStart = nBlockStart - ((GetBudgetPaymentCycleBlocks() / 30) * 2);
    int nOffsetToStart = nFinalizationStart - nCurrentHeight;

    if (nBlockStart - nCurrentHeight > ((GetBudgetPaymentCycleBlocks() / 30) * 2)){
        LogPrint("masternode","CBudgetManager::SubmitFinalBudget - Too early for finalization. Current block is %ld, next Superblock is %ld.\n", nCurrentHeight, nBlockStart);
        LogPrint("masternode","CBudgetManager::SubmitFinalBudget - First possible block for finalization: %ld. Last possible block for finalization: %ld. You have to wait for %ld block(s) until Budget finalization will be possible\n", nFinalizationStart, nBlockStart, nOffsetToStart);

        return;
    }

    std::vector<CBudgetProposal*> vBudgetProposals = budget.GetBudget();
    std::string strBudgetName = "main";
    std::vector<CTxBudgetPayment> vecTxBudgetPayments;

    for (unsigned int i = 0; i < vBudgetProposals.size(); i++) {
        CTxBudgetPayment txBudgetPayment;
        txBudgetPayment.nProposalHash = vBudgetProposals[i]->GetHash();
        txBudgetPayment.payee = vBudgetProposals[i]->GetPayee();
        txBudgetPayment.nAmount = vBudgetProposals[i]->GetAllotted();
        vecTxBudgetPayments.push_back(txBudgetPayment);
    }

    if (vecTxBudgetPayments.size() < 1) {
        LogPrint("masternode","CBudgetManager::SubmitFinalBudget - Found No Proposals For Period\n");
        return;
    }

    CFinalizedBudgetBroadcast tempBudget(strBudgetName, nBlockStart, vecTxBudgetPayments, uint256());
    if (mapSeenFinalizedBudgets.count(tempBudget.GetHash())) {
        LogPrint("masternode","CBudgetManager::SubmitFinalBudget - Budget already exists - %s\n", tempBudget.GetHash().ToString());
        nSubmittedHeight = nCurrentHeight;
        return; //already exists
    }

    //create fee tx
    CTransaction tx;
    uint256 txidCollateral;

    if (!mapCollateralTxids.count(tempBudget.GetHash())) {
        CWalletTx wtx;
        if (!pwalletMain->GetBudgetSystemCollateralTX(wtx, tempBudget.GetHash(), false)) {
            LogPrint("masternode","CBudgetManager::SubmitFinalBudget - Can't make collateral transaction\n");
            return;
        }

        // Get our change address
        CReserveKey reservekey(pwalletMain);
        // Send the tx to the network. Do NOT use SwiftTx, locking might need too much time to propagate, especially for testnet
        pwalletMain->CommitTransaction(wtx, reservekey, "NO-ix");
        tx = (CTransaction)wtx;
        txidCollateral = tx.GetHash();
        mapCollateralTxids.insert(make_pair(tempBudget.GetHash(), txidCollateral));
    } else {
        txidCollateral = mapCollateralTxids[tempBudget.GetHash()];
    }

    int conf = GetIXConfirmations(txidCollateral);
    CTransaction txCollateral;
    uint256 nBlockHash;

    if (!GetTransaction(txidCollateral, txCollateral, nBlockHash, true)) {
        LogPrint("masternode","CBudgetManager::SubmitFinalBudget - Can't find collateral tx %s", txidCollateral.ToString());
        return;
    }

    if (nBlockHash != uint256()) {
        BlockMap::iterator mi = mapBlockIndex.find(nBlockHash);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex* pindex = (*mi).second;
            if (chainActive.Contains(pindex)) {
                conf += chainActive.Height() - pindex->nHeight + 1;
            }
        }
    }

    /*
        Wait will we have 1 extra confirmation, otherwise some clients might reject this feeTX
        -- This function is tied to NewBlock, so we will propagate this budget while the block is also propagating
    */
    if (conf < Params().Budget_Fee_Confirmations() + 1) {
        LogPrint("masternode","CBudgetManager::SubmitFinalBudget - Collateral requires at least %d confirmations - %s - %d confirmations\n", Params().Budget_Fee_Confirmations() + 1, txidCollateral.ToString(), conf);
        return;
    }

    //create the proposal incase we're the first to make it
    CFinalizedBudgetBroadcast finalizedBudgetBroadcast(strBudgetName, nBlockStart, vecTxBudgetPayments, txidCollateral);

    std::string strError = "";
    if (!finalizedBudgetBroadcast.IsValid(strError)) {
        LogPrint("masternode","CBudgetManager::SubmitFinalBudget - Invalid finalized budget - %s \n", strError);
        return;
    }

    LOCK(cs);
    mapSeenFinalizedBudgets.insert(make_pair(finalizedBudgetBroadcast.GetHash(), finalizedBudgetBroadcast));
    finalizedBudgetBroadcast.Relay();
    budget.AddFinalizedBudget(finalizedBudgetBroadcast);
    nSubmittedHeight = nCurrentHeight;
    LogPrint("masternode","CBudgetManager::SubmitFinalBudget - Done! %s\n", finalizedBudgetBroadcast.GetHash().ToString());
}

//
// CBudgetDB
//

CBudgetDB::CBudgetDB()
{
    pathDB = GetDataDir() / "budget.dat";
    strMagicMessage = "MasternodeBudget";
}

bool CBudgetDB::Write(const CBudgetManager& objToSave)
{
    LOCK(objToSave.cs);

    int64_t nStart = GetTimeMillis();

    // serialize, checksum data up to that point, then append checksum
    CDataStream ssObj(SER_DISK, CLIENT_VERSION);
    ssObj << strMagicMessage;                   // masternode cache file specific magic message
    ssObj << FLATDATA(Params().MessageStart()); // network specific magic number
    ssObj << objToSave;
    uint256 hash = Hash(ssObj.begin(), ssObj.end());
    ssObj << hash;

    // open output file, and associate with CAutoFile
    FILE* file = fopen(pathDB.string().c_str(), "wb");
    CAu