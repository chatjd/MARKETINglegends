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
    int64_t nTimeDepth = 20; //w