
// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "core_io.h"
#ifdef ENABLE_MINING
#include "crypto/equihash.h"
#endif
#include "init.h"
#include "main.h"
#include "metrics.h"
#include "miner.h"
#include "net.h"
#include "pow.h"
#include "rpc/server.h"
#include "txmempool.h"
#include "util.h"
#include "validationinterface.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include "key_io.h"

#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include <univalue.h>

using namespace std;

/**
 * Return average network hashes per second based on the last 'lookup' blocks,
 * or over the difficulty averaging window if 'lookup' is nonpositive.
 * If 'height' is nonnegative, compute the estimate at the time when a given block was found.
 */
int64_t GetNetworkHashPS(int lookup, int height) {
    CBlockIndex *pb = chainActive.Tip();

    if (height >= 0 && height < chainActive.Height())
        pb = chainActive[height];

    if (pb == NULL || !pb->nHeight)
        return 0;

    // If lookup is nonpositive, then use difficulty averaging window.
    if (lookup <= 0)
        lookup = Params().GetConsensus().nPowAveragingWindow;

    // If lookup is larger than chain, then set it to chain length.
    if (lookup > pb->nHeight)
        lookup = pb->nHeight;

    CBlockIndex *pb0 = pb;
    int64_t minTime = pb0->GetBlockTime();
    int64_t maxTime = minTime;
    for (int i = 0; i < lookup; i++) {
        pb0 = pb0->pprev;
        int64_t time = pb0->GetBlockTime();
        minTime = std::min(time, minTime);
        maxTime = std::max(time, maxTime);
    }

    // In case there's a situation where minTime == maxTime, we don't want a divide by zero exception.
    if (minTime == maxTime)
        return 0;

    arith_uint256 workDiff = pb->nChainWork - pb0->nChainWork;
    int64_t timeDiff = maxTime - minTime;

    return (int64_t)(workDiff.getdouble() / timeDiff);
}

UniValue getlocalsolps(const UniValue& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error(
            "getlocalsolps\n"
            "\nReturns the average local solutions per second since this node was started.\n"
            "This is the same information shown on the metrics screen (if enabled).\n"
            "\nResult:\n"
            "xxx.xxxxx     (numeric) Solutions per second average\n"
            "\nExamples:\n"
            + HelpExampleCli("getlocalsolps", "")
            + HelpExampleRpc("getlocalsolps", "")
       );

    LOCK(cs_main);
    return GetLocalSolPS();
}

UniValue getnetworksolps(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "getnetworksolps ( blocks height )\n"
            "\nReturns the estimated network solutions per second based on the last n blocks.\n"
            "Pass in [blocks] to override # of blocks, -1 specifies over difficulty averaging window.\n"
            "Pass in [height] to estimate the network speed at the time when a certain block was found.\n"
            "\nArguments:\n"
            "1. blocks     (numeric, optional, default=120) The number of blocks, or -1 for blocks over difficulty averaging window.\n"
            "2. height     (numeric, optional, default=-1) To estimate at the time of the given height.\n"
            "\nResult:\n"
            "x             (numeric) Solutions per second estimated\n"
            "\nExamples:\n"
            + HelpExampleCli("getnetworksolps", "")
            + HelpExampleRpc("getnetworksolps", "")
       );

    LOCK(cs_main);
    return GetNetworkHashPS(params.size() > 0 ? params[0].get_int() : 120, params.size() > 1 ? params[1].get_int() : -1);
}

UniValue getnetworkhashps(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "getnetworkhashps ( blocks height )\n"
            "\nDEPRECATED - left for backwards-compatibility. Use getnetworksolps instead.\n"
            "\nReturns the estimated network solutions per second based on the last n blocks.\n"
            "Pass in [blocks] to override # of blocks, -1 specifies over difficulty averaging window.\n"
            "Pass in [height] to estimate the network speed at the time when a certain block was found.\n"
            "\nArguments:\n"
            "1. blocks     (numeric, optional, default=120) The number of blocks, or -1 for blocks over difficulty averaging window.\n"
            "2. height     (numeric, optional, default=-1) To estimate at the time of the given height.\n"
            "\nResult:\n"
            "x             (numeric) Solutions per second estimated\n"
            "\nExamples:\n"
            + HelpExampleCli("getnetworkhashps", "")
            + HelpExampleRpc("getnetworkhashps", "")
       );

    LOCK(cs_main);
    return GetNetworkHashPS(params.size() > 0 ? params[0].get_int() : 120, params.size() > 1 ? params[1].get_int() : -1);
}

#ifdef ENABLE_MINING
UniValue getgenerate(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getgenerate\n"
            "\nReturn if the server is set to generate coins or not. The default is false.\n"
            "It is set with the command line argument -gen (or vidulum.conf setting gen)\n"
            "It can also be set with the setgenerate call.\n"
            "\nResult\n"
            "true|false      (boolean) If the server is set to generate coins or not\n"
            "\nExamples:\n"
            + HelpExampleCli("getgenerate", "")
            + HelpExampleRpc("getgenerate", "")
        );

    LOCK(cs_main);
    return GetBoolArg("-gen", false);
}

UniValue generate(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "generate numblocks\n"
            "\nMine blocks immediately (before the RPC call returns)\n"
            "\nNote: this function can only be used on the regtest network\n"
            "\nArguments:\n"
            "1. numblocks    (numeric) How many blocks are generated immediately.\n"
            "\nResult\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks\n"
            + HelpExampleCli("generate", "11")
        );

    if (GetArg("-mineraddress", "").empty()) {
#ifdef ENABLE_WALLET
        if (!pwalletMain) {
            throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Wallet disabled and -mineraddress not set");
        }
#else
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "vidulumd compiled without wallet and -mineraddress not set");
#endif
    }
    if (!Params().MineBlocksOnDemand())
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "This method can only be used on regtest");

    int nHeightStart = 0;
    int nHeightEnd = 0;
    int nHeight = 0;
    int nGenerate = params[0].get_int();
#ifdef ENABLE_WALLET
    CReserveKey reservekey(pwalletMain);
#endif

    {   // Don't keep cs_main locked
        LOCK(cs_main);
        nHeightStart = chainActive.Height();
        nHeight = nHeightStart;
        nHeightEnd = nHeightStart+nGenerate;
    }
    unsigned int nExtraNonce = 0;
    UniValue blockHashes(UniValue::VARR);



    EHparameters ehparams[MAX_EH_PARAM_LIST_LEN]; //allocate on-stack space for parameters list
    const CChainParams& chainparams = Params();
    
    while (nHeight < nHeightEnd)
    {
            validEHparameterList(ehparams,nHeight+1,chainparams);
            unsigned int n = ehparams[0].n;
            unsigned int k = ehparams[0].k;


#ifdef ENABLE_WALLET
        std::unique_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey(reservekey));
#else
        std::unique_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey());
#endif
        if (!pblocktemplate.get())
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wallet keypool empty");
        CBlock *pblock = &pblocktemplate->block;
        {
            LOCK(cs_main);
            IncrementExtraNonce(pblock, chainActive.Tip(), nExtraNonce);
        }

        // Hash state
        crypto_generichash_blake2b_state eh_state;
        EhInitialiseState(n, k, eh_state);

        // I = the block header minus nonce and solution.
        CEquihashInput I{*pblock};
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << I;

        // H(I||...
        crypto_generichash_blake2b_update(&eh_state, (unsigned char*)&ss[0], ss.size());

        while (true) {
            // Yes, there is a chance every nonce could fail to satisfy the -regtest
            // target -- 1 in 2^(2^256). That ain't gonna happen
            pblock->nNonce = ArithToUint256(UintToArith256(pblock->nNonce) + 1);

            // H(I||V||...
            crypto_generichash_blake2b_state curr_state;
            curr_state = eh_state;
            crypto_generichash_blake2b_update(&curr_state,
                                              pblock->nNonce.begin(),
                                              pblock->nNonce.size());

            // (x_1, x_2, ...) = A(I, V, n, k)
            std::function<bool(std::vector<unsigned char>)> validBlock =
                    [&pblock](std::vector<unsigned char> soln) {
                pblock->nSolution = soln;
                solutionTargetChecks.increment();
                return CheckProofOfWork(pblock->GetHash(), pblock->nBits, Params().GetConsensus());
            };
            bool found = EhBasicSolveUncancellable(n, k, curr_state, validBlock);
            ehSolverRuns.increment();
            if (found) {
                goto endloop;
            }
        }
endloop:
        CValidationState state;
        if (!ProcessNewBlock(state, NULL, pblock, true, NULL))
            throw JSONRPCError(RPC_INTERNAL_ERROR, "ProcessNewBlock, block not accepted");
        ++nHeight;
        blockHashes.push_back(pblock->GetHash().GetHex());
    }
    return blockHashes;
}


UniValue setgenerate(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setgenerate generate ( genproclimit )\n"
            "\nSet 'generate' true or false to turn generation on or off.\n"
            "Generation is limited to 'genproclimit' processors, -1 is unlimited.\n"
            "See the getgenerate call for the current setting.\n"
            "\nArguments:\n"
            "1. generate         (boolean, required) Set to true to turn on generation, off to turn off.\n"
            "2. genproclimit     (numeric, optional) Set the processor limit for when generation is on. Can be -1 for unlimited.\n"
            "\nExamples:\n"
            "\nSet the generation on with a limit of one processor\n"
            + HelpExampleCli("setgenerate", "true 1") +
            "\nCheck the setting\n"
            + HelpExampleCli("getgenerate", "") +
            "\nTurn off generation\n"
            + HelpExampleCli("setgenerate", "false") +
            "\nUsing json rpc\n"
            + HelpExampleRpc("setgenerate", "true, 1")
        );

    if (GetArg("-mineraddress", "").empty()) {
#ifdef ENABLE_WALLET
        if (!pwalletMain) {
            throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Wallet disabled and -mineraddress not set");
        }
#else
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "vidulumd compiled without wallet and -mineraddress not set");
#endif
    }
    if (Params().MineBlocksOnDemand())
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Use the generate method instead of setgenerate on this network");

    bool fGenerate = true;
    if (params.size() > 0)
        fGenerate = params[0].get_bool();

    int nGenProcLimit = -1;
    if (params.size() > 1)
    {
        nGenProcLimit = params[1].get_int();
        if (nGenProcLimit == 0)
            fGenerate = false;
    }

    mapArgs["-gen"] = (fGenerate ? "1" : "0");
    mapArgs ["-genproclimit"] = itostr(nGenProcLimit);
#ifdef ENABLE_WALLET
    GenerateBitcoins(fGenerate, pwalletMain, nGenProcLimit);
#else
    GenerateBitcoins(fGenerate, nGenProcLimit);
#endif

    return NullUniValue;
}
#endif


UniValue getmininginfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmininginfo\n"
            "\nReturns a json object containing mining-related information."
            "\nResult:\n"
            "{\n"
            "  \"blocks\": nnn,             (numeric) The current block\n"
            "  \"currentblocksize\": nnn,   (numeric) The last block size\n"
            "  \"currentblocktx\": nnn,     (numeric) The last block transaction\n"
            "  \"difficulty\": xxx.xxxxx    (numeric) The current difficulty\n"
            "  \"errors\": \"...\"          (string) Current errors\n"
            "  \"generate\": true|false     (boolean) If the generation is on or off (see getgenerate or setgenerate calls)\n"
            "  \"genproclimit\": n          (numeric) The processor limit for generation. -1 if no generation. (see getgenerate or setgenerate calls)\n"
            "  \"localsolps\": xxx.xxxxx    (numeric) The average local solution rate in Sol/s since this node was started\n"
            "  \"networksolps\": x          (numeric) The estimated network solution rate in Sol/s\n"
            "  \"pooledtx\": n              (numeric) The size of the mem pool\n"
            "  \"testnet\": true|false      (boolean) If using testnet or not\n"
            "  \"chain\": \"xxxx\",         (string) current network name as defined in BIP70 (main, test, regtest)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmininginfo", "")
            + HelpExampleRpc("getmininginfo", "")
        );


    LOCK(cs_main);

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("blocks",           (int)chainActive.Height()));
    obj.push_back(Pair("currentblocksize", (uint64_t)nLastBlockSize));
    obj.push_back(Pair("currentblocktx",   (uint64_t)nLastBlockTx));
    obj.push_back(Pair("difficulty",       (double)GetNetworkDifficulty()));
    obj.push_back(Pair("errors",           GetWarnings("statusbar")));
    obj.push_back(Pair("genproclimit",     (int)GetArg("-genproclimit", -1)));
    obj.push_back(Pair("localsolps"  ,     getlocalsolps(params, false)));
    obj.push_back(Pair("networksolps",     getnetworksolps(params, false)));
    obj.push_back(Pair("networkhashps",    getnetworksolps(params, false)));
    obj.push_back(Pair("pooledtx",         (uint64_t)mempool.size()));
    obj.push_back(Pair("testnet",          Params().TestnetToBeDeprecatedFieldRPC()));
    obj.push_back(Pair("chain",            Params().NetworkIDString()));
#ifdef ENABLE_MINING
    obj.push_back(Pair("generate",         getgenerate(params, false)));
#endif
    return obj;