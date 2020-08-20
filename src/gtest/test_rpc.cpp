#include <gtest/gtest.h>
#include <univalue.h>

#include "chain.h"
#include "chainparams.h"
#include "clientversion.h"
#include "primitives/block.h"
#include "rpc/server.h"
#include "streams.h"
#include "utilstrencodings.h"

extern UniValue blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool txDetails = false);

TEST(rpc, check_blockToJSON_returns_minified_solution) {
    SelectParams(CBaseChainParams::TESTNET);

    // Testnet block 006a87f9f91c1f51c7549e2c8965c0fd4fe8c212798f932efc54dc7bccbec780
    // Height 1391
    CDataStream ss(ParseHex("0400000077be515306e347c6856686d83a229169140a2f7e17281c8319ecf00c49bb6f00994ca400914d6733295faf4e0063998e75a18aae7d39b5244d88d082c13145070000000000000000000000000000000000000000000000000000000000000000ae71c25700737b1f010090f8a62f53105d6b6f173d242fbbf54b0c1024a64520f0020e47fe710000fd4005009f44ff7505d789b964d6817734b8ce1377d456255994370d06e59ac99bd5791b6ad174a66fd71c70e60cfc7fd88243ffe06f80b1ad181625f210779c745524629448e25348a5fce4f346a1735e60fdf53e144c0157dbc47c700a21a236f1efb7ee75f65b8d9d9e29026cfd09048233175202b211b9a49de4ab46f1cac71b6ea57a686377bd612378746e70c61a659c9cd683269e9c2a5cbc1d19f1149345302bbd0a1e62bf4bab01e9caeea789a1519441