// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "policy/fees.h"

#include "amount.h"
#include "primitives/transaction.h"
#include "streams.h"
#include "txmempool.h"
#include "util.h"

void TxConfirmStats::Initialize(std::vector<double>& defaultBuckets,
                                unsigned int maxConfirms, double _decay, std::string _dataTypeString)
{
    decay = _decay;
    dataTypeString = _dataTypeString;

    buckets.insert(buckets.end(), defaultBuckets.begin(), defaultBuckets.end());
    buckets.push_back(std::numeric_limits<double>::infinity());

    for (unsigned int i = 0; i < buckets.size(); i++) {
        bucketMap[buckets[i]] = i;
    }

    confAvg.resize(maxConfirms);
    curBlockConf.resize(maxConfirms);
    unconfTxs.resize(maxConfirms);
    for (unsigned int i = 0; i < maxConfirms; i++) {
        confAvg[i].resize(buckets.size());
        curBlockConf[i].resize(buckets.size());
        unconfTxs[i].resize(buckets.size());
    }

    oldUnconfTxs.resize(buckets.size());
    curBlockTxCt.resize(buckets.size());
    txCtAvg.resize(buckets.size());
    curBlockVal.resize(buckets.size());
    avg.resize(buckets.size());
}

// Zero out the data for the current block
void TxConfirmStats::ClearCurrent(unsigned int nBlockHeight)
{
    for (unsigned int j = 0; j < buckets.size(); j++) {
        oldUnconfTxs[j] += unconfTxs[nBlockHeight%unconfTxs.size()][j];
        unconfTxs[nBlockHeight%unconfTxs.size()][j] = 0;
        for (unsigned int i = 0; i < curBlockConf.size(); i++)
            curBlockConf[i][j] = 0;
        curBlockTxCt[j] = 0;
        curBlockVal[j] = 0;
    }
}

unsigned int TxConfirmStats::FindBucketIndex(double val)
{
    auto it = bucketMap.lower_bound(val);
    assert(it != bucketMap.end());
    return it->second;
}

void TxConfirmStats::Record(int blocksToConfirm, double val)
{
    // blocksToConfirm is 1-based
    if (blocksToConfirm < 1)
        return;
    unsigned int bucketindex = FindBucketIndex(val);
    for (size_t i = blocksToConfirm; i <= curBlockConf.size(); i++) {
        curBlockConf[i - 1][bucketindex]++;
    }
    curBlockTxCt[bucketindex]++;
    curBlockVal[bucketindex] += val;
}

void TxConfirmStats::UpdateMovingAverages()
{
    for (unsigned int j = 0; j < buckets.size(); j++) {
        for (unsigned int i = 0; i < confAvg.size(); i++)
            confAvg[i][j] = confAvg[i][j] * decay + curBlockConf[i][j];
        avg[j] = avg[j] * decay + curBlockVal[j];
        txCtAvg[j] = txCtAvg[j] * decay + curBlockTxCt[j];
    }
}

// returns -1 on error conditions
double TxConfirmStats::EstimateMedianVal(int confTarget, double sufficientTxVal,
                                         double successBreakPoint, bool r