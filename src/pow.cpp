// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "chainparams.h"
#include "rpc/server.h" // GetDifficulty()
#include "crypto/equihash.h"
#include "primitives/block.h"
#include "streams.h"
#include "uint256.h"
#include "util.h"

#include "sodium.h"

unsigned int static LDAv1NextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params) {
    /* Vidulum - Liquid Difficulty Algorithm (LDAv1), written by Corey Miller - coreym@vidulum.org */
    // LogPrint("pow", "Liquid Difficulty Algo\n");

    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    const int64_t height = pindexLast->nHeight;
    int64_t nLiquidDepth = 120;
    int64_t nTimeDepth = 20; //work with time deltas between last nTimeDepth blocks

    //  TODO: SET THIS TO A DEFAULT DIFF FOR THE nLiquidDepth BLOCK WINDOW
    // reset diff over first nLiquidDepth blocks after LDA activation
    if (height < (params.vUpgrades[Consensus::UPGRADE_LIQUID].nActivationHeight + nLiquidDepth)) {
       //  TODO: MIGHT MOVE THIS TO CHAINPARAMS
       // Should be about 105 diff
       const arith_uint256 bnLiquidPowLimit = UintToArith256(uint256S("0007ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));
       return bnLiquidPowLimit.GetCompact();
    }

    arith_uint256 bnTargetTotal;
    const CBlockIndex *pindex = pindexLast;

    int64_t nWeightedTimespan = 0;
    int64_t nTimeWeightFactor = (nTimeDepth / 2) * (1 + nTimeDepth); 
    int64_t nTargetTimespan = nTimeWeightFactor * params.nPowTargetSpacing;

    // Aquire target avg over nLiquidDepth and weighted solve time over nTimeDepth
    for (unsigned int nCountBlocks = 1; nCountBlocks <= nLiquidDepth; nCountBlocks++) {
        arith_uint256 bnWork = arith_uint256().SetCompact(pindex->nBits);
        bnTargetTotal += bnWork;

        int64_t nFirstDelta = pindex->GetBlockTime();
        
        assert(pindex->pprev); // should never fail
        pindex = pindex->pprev;

        if(nTimeDepth >= 1){
            int64_t nTimeDelta = nFirstDelta - pindex->GetBlockTime();
            // LogPrint("pow", "Diff: %f   Height: %d   tDelta: %d \n", GetDifficulty(pindex), pindex->nHeight, nTimeDelta);

            // We care about actual and target time but let's not care too much
            if (nTimeDelta <= params.nPowTargetSpacing/10){
                nTimeDelta = params.nPowTargetSpacing/10;
                // LogPrint("pow", "very low nTimeDelta override to: %d \n", nTimeDelta);
            }
            if (nTimeDelta >= params.nPowTargetSpacing*10){
                nTimeDelta = params.nPowTargetSpacing*10;
                // LogPrint("pow", "very high nTimeDelta override to: %d \n", nTimeDelta);
            }

            // Add weight to more important time deltas
            nWeightedTimespan += (nTimeDelta * nTimeDepth);
            nTimeDepth--;
        }
    }

    bnTargetTotal = bnTargetTotal / (nLiquidDepth * 2);
    arith_uint256 bnPrevTargetAvg = bnTargetTotal * 2;

    // LogPrint("pow", "nTargetTimespan: %d \n", nTargetTimespan);
    // LogPrint("pow", "nWeightedTimespan: %d \n", nWeightedTimespan);
    double nRetargetFactor = (nWeightedTimespan / double(nTargetTimespan) * 100);
    // LogPrint("pow", "nRetargetFactor: %d \n", nRetargetFactor);

    arith_uint256 bnLastTarget = arith_uint256().SetCompact(pindexLast->nBits);
    arith_uint256 bnNextDiff = bnLastTarget;
    if (nRetargetFactor >= 150) {
        // LogPrint("pow", "nRetargetFactor > 150 override to: 150 of prev target\n");
        bnNextDiff = (bnLastTarget * 150) / 100;
    } else if (nRetargetFactor <= 67) {
        // LogPrint("pow", "nRetargetFactor < 67 override to: 67 of prev target\n");
        bnNextDiff = (bnLastTarget * 67) / 100;
    } else if (nRetargetFactor < 83 || nRetargetFactor > 120) {
        // LogPrint("pow", "nRetargetFactor < 83 or > 120 adjust by bnPrevTargetAvg\n");
        bnNextDiff = (bnPrevTargetAvg * nRetargetFactor) / 100;
    }
    // else{
        // LogPrint("pow", "Timing looks great - use last target \n");
    // }

    if (bnNextDiff > bnPowLimit) {
        // LogPrint("pow", "bnNextDiff more than bnPowLimit - Resetting \n");
        bnNextDiff = bnPowLimit;
    }

    return bnNextDiff.GetCompact();
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    const CChainParams& chainParams = Params();
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    // Reset the difficulty after the algo fork
    if (pindexLast->nHeight >= chainParams.eh_epoch_1_end() - params.nPowAveragingWindow
        && pindexLast->nHeight < chainParams.eh_epoch_1_end()) {
        LogPrint("pow", "Reset the difficulty for the eh_epoch_2 algo change: %d\n", nProofOfWorkLimit);
        return nProofOfWorkLimit;
    }

	{
        // Comparing to pindexLast->nHeight with >= because this function
        // returns the work required for the block after pindexLast.
        if (params.nPowAllowMinDifficultyBlocksAfterHeight != boost::none &&
            pindexLast->nHeight >= params.nPowAllowMinDifficultyBlocksAfterHeight.get())
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 6 * 2.5 minutes
            // then allow mining of a min-difficulty block.
            if (pblock && pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing * 6)
                return nProofOfWorkLimit;
        }
    }

    // Find the first block in the averaging interval
    const CBlockIndex* pindexFirst = pindexLast;
    arith_uint256 bnTot {0};
    for (int i = 0; pindexFirst && i < params.nPowAveragingWindow; i++) {
        arith_uint256 bnTmp;
        bnTmp.SetCompact(pindexFirst->nBits);
        bnTot += bnTmp;
        pindexFirst = pindexFirst->pprev;
    }

    // Check we have enough blocks
    if (pindexFirst == NULL)
        return nProofOfWorkLimit;

    arith_uint256 bnAvg {bnTot / params.nPowAveragingWindow};

    //Difficulty algo
    int nHeight = pindexLast->nHeight + 1;
    if (nHeight < params.vUpgrades[Consensus::UPGRADE_DIFA].nActivationHeight) {
        return DigishieldCalculateNextWorkRequired(bnAvg, pindexLast->GetMedianTimePast(), pindexFirst->GetMedianTimePast(), params);
    } else if(nHeight < params.vUpgrades[Consensus::UPGRADE_LIQUID].nActivationHeight) {
        return Lwma3CalculateNextWorkRequired(pindexLast, params);
    } else {
        return LDAv1NextWorkRequired(pindexLast, pblock, params);
    }
}

unsigned int DigishieldCalculateNextWorkRequired(arith_uint256 bnAvg,
                                       int64_t nLastBlockTime, int64_t nFirstBlockTime,
                                       const Consensus::