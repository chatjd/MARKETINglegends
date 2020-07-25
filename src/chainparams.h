// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAINPARAMS_H
#define BITCOIN_CHAINPARAMS_H

#include "chainparamsbase.h"
#include "consensus/params.h"
#include "primitives/block.h"
#include "protocol.h"

#include <vector>

struct CDNSSeedData {
    std::string name, host;
    CDNSSeedData(const std::string &strName, const std::string &strHost) : name(strName), host(strHost) {}
};

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

struct EHparameters {
    unsigned char n;
    unsigned char k;
    unsigned short int nSolSize;
};

//EH sol size = (pow(2, k) * ((n/(k+1))+1)) / 8;
static const EHparameters eh200_9 = {200,9,1344};
static const EHparameters eh192_7 = {192,7,400};
static const EHparameters eh144_5 = {144,5,100};
static const EHparameters eh96_5 = {96,5,68};
static const EHparameters eh48_5 = {48,5,36};
static const unsigned int MAX_EH_PARAM_LIST_LEN = 2;

typedef std::map<int, uint256> MapCheckpoints;

struct CCheckpointData {
    MapCheckpoints mapCheckpoints;
    int64_t nTimeLastCheckpoint;
    int64_t nTransactionsLastCheckpoint;
    double fTransactionsPerDay;
};

/**
 * CChainParams defines various tweakable parameters of a given instance of the
 * Bitcoin system. There are three: the main network on which people trade goods
 * and services, the public test network which gets reset from time to time and
 * a regression test mode which is intended for private networks only. It has
 * minimal difficulty to ensure that blocks can be found instantly.
 */
class CChainParams
{
public:
    enum Base58Type {
        PUBKEY_ADDRESS,
        SCRIPT_ADDRESS,
        SECRET_KEY,
        EXT_PUBLIC_KEY,
  