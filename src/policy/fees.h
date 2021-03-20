
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_POLICYESTIMATOR_H
#define BITCOIN_POLICYESTIMATOR_H

#include "amount.h"
#include "uint256.h"

#include <map>
#include <string>
#include <vector>

class CAutoFile;
class CFeeRate;
class CTxMemPoolEntry;

/** \class CBlockPolicyEstimator
 * The BlockPolicyEstimator is used for estimating the fee or priority needed
 * for a transaction to be included in a block within a certain number of
 * blocks.
 *
 * At a high level the algorithm works by grouping transactions into buckets
 * based on having similar priorities or fees and then tracking how long it
 * takes transactions in the various buckets to be mined.  It operates under
 * the assumption that in general transactions of higher fee/priority will be
 * included in blocks before transactions of lower fee/priority.   So for
 * example if you wanted to know what fee you should put on a transaction to
 * be included in a block within the next 5 blocks, you would start by looking
 * at the bucket with with the highest fee transactions and verifying that a
 * sufficiently high percentage of them were confirmed within 5 blocks and
 * then you would look at the next highest fee bucket, and so on, stopping at
 * the last bucket to pass the test.   The average fee of transactions in this
 * bucket will give you an indication of the lowest fee you can put on a
 * transaction and still have a sufficiently high chance of being confirmed
 * within your desired 5 blocks.
 *
 * When a transaction enters the mempool or is included within a block we
 * decide whether it can be used as a data point for fee estimation, priority
 * estimation or neither.  If the value of exactly one of those properties was
 * below the required minimum it can be used to estimate the other.  In
 * addition, if a priori our estimation code would indicate that the
 * transaction would be much more quickly included in a block because of one
 * of the properties compared to the other, we can also decide to use it as
 * an estimate for that property.
 *
 * Here is a brief description of the implementation for fee estimation.
 * When a transaction that counts for fee estimation enters the mempool, we
 * track the height of the block chain at entry.  Whenever a block comes in,
 * we count the number of transactions in each bucket and the total amount of fee
 * paid in each bucket. Then we calculate how many blocks Y it took each
 * transaction to be mined and we track an array of counters in each bucket
 * for how long it to took transactions to get confirmed from 1 to a max of 25
 * and we increment all the counters from Y up to 25. This is because for any
 * number Z>=Y the transaction was successfully mined within Z blocks.  We
 * want to save a history of this information, so at any time we have a
 * counter of the total number of transactions that happened in a given fee
 * bucket and the total number that were confirmed in each number 1-25 blocks
 * or less for any bucket.   We save this history by keeping an exponentially
 * decaying moving average of each one of these stats.  Furthermore we also
 * keep track of the number unmined (in mempool) transactions in each bucket
 * and for how many blocks they have been outstanding and use that to increase
 * the number of transactions we've seen in that fee bucket when calculating
 * an estimate for any number of confirmations below the number of blocks
 * they've been outstanding.
 */

/** Decay of .998 is a half-life of 346 blocks or about 2.4 days */
static const double DEFAULT_DECAY = .998;

/**
 * We will instantiate two instances of this class, one to track transactions
 * that were included in a block due to fee, and one for tx's included due to
 * priority.  We will lump transactions into a bucket according to their approximate
 * fee or priority and then track how long it took for those txs to be included
 * in a block. There is always a bucket into which any given double value
 * (representing a fee or priority) falls.
 *
 * The tracking of unconfirmed (mempool) transactions is completely independent of the
 * historical tracking of transactions that have been confirmed in a block.
 */
class TxConfirmStats
{
private:
    //Define the buckets we will group transactions into (both fee buckets and priority buckets)
    std::vector<double> buckets;              // The upper-bound of the range for the bucket (inclusive)
    std::map<double, unsigned int> bucketMap; // Map of bucket upper-bound to index into all vectors by bucket

    // For each bucket X:
    // Count the total # of txs in each bucket
    // Track the historical moving average of this total over blocks
    std::vector<double> txCtAvg;
    // and calcuate the total for the current block to update the moving average
    std::vector<int> curBlockTxCt;

    // Count the total # of txs confirmed within Y blocks in each bucket
    // Track the historical moving average of theses totals over blocks
    std::vector<std::vector<double> > confAvg; // confAvg[Y][X]
    // and calcuate the totals for the current block to update the moving averages
    std::vector<std::vector<int> > curBlockConf; // curBlockConf[Y][X]

    // Sum the total priority/fee of all tx's in each bucket
    // Track the historical moving average of this total over blocks
    std::vector<double> avg;
    // and calculate the total for the current block to update the moving average
    std::vector<double> curBlockVal;

    // Combine the conf counts with tx counts to calculate the confirmation % for each Y,X
    // Combine the total value with the tx counts to calculate the avg fee/priority per bucket

    std::string dataTypeString;
    double decay = DEFAULT_DECAY;

    // Mempool counts of outstanding transactions
    // For each bucket X, track the number of transactions in the mempool
    // that are unconfirmed for each possible confirmation value Y
    std::vector<std::vector<int> > unconfTxs;  //unconfTxs[Y][X]