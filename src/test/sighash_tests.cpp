// Copyright (c) 2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "data/sighash.json.h"
#include "main.h"
#include "random.h"
#include "script/interpreter.h"
#include "script/script.h"
#include "serialize.h"
#include "test/test_bitcoin.h"
#include "util.h"
#include "version.h"
#include "sodium.h"

#include <iostream>
#include <random>

#include <boost/test/unit_test.hpp>

#include <univalue.h>

extern UniValue read_json(const std::string& jsondata);

// Old script.cpp SignatureHash function
uint256 static SignatureHashOld(CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType)
{
    static const uint256 one(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    if (nIn >= txTo.vin.size())
    {
        printf("ERROR: SignatureHash(): nIn=%d out of range\n", nIn);
        return one;
    }
    CMutableTransaction txTmp(txTo);

    // Blank out other inputs' signatures
    for (unsigned int i = 0; i < txTmp.vin.size(); i++)
        txTmp.vin[i].scriptSig = CScript();
    txTmp.vin[nIn].scriptSig = scriptCode;

    // Blank out some of the outputs
    if ((nHashType & 0x1f) == SIGHASH_NONE)
    {
        // Wildcard payee
        txTmp.vout.clear();

        // Let the others update at will
        for (unsigned int i = 0; i < txTmp.vin.size(); i++)
            if (i != nIn)
                txTmp.vin[i].nSequence = 0;
    }
    else if ((nHashType & 0x1f) == SIGHASH_SINGLE)
    {
        // Only lock-in the txout payee at same index as txin
        unsigned int nOut = nIn;
        if (nOut >= txTmp.vout.size())
        {
            printf("ERROR: SignatureHash(): nOut=%d out of range\n", nOut);
            return one;
        }
        txTmp.vout.resize(nOut+1);
        for (unsigned int i = 0; i < nOut; i++)
            txTmp.vout[i].SetNull();

        // Let the others update at will
        for (unsigned int i = 0; i < txTmp.vin.size(); i++)
            if (i != nIn)
                txTmp.vin[i].nSequence = 0;
    }

    // Blank out other inputs completely, not recommended for open transactions
    if (nHashType & SIGHASH_ANYONECANPAY)
    {
        txTmp.vin[0] = txTmp.vin[nIn];
        txTmp.vin.resize(1);
    }

    // Blank out the joinsplit signature.
    memset(&txTmp.joinSplitSig[0], 0, txTmp.joinSplitSig.size());

    // Serialize and hash
    CHashWriter ss(SER_GETHASH, 0);
    ss << txTmp << nHashType;
    return ss.GetHash();
}

void static RandomScript(CScript &script) {
    static const opcodetype oplist[] = {OP_FALSE, OP_1, OP_2, OP_3, OP_CHECKSIG, OP_IF, OP_VERIF, OP_RETURN};
    script = CScript();
    int ops = (insecure_rand() % 10);
    for (int i=0; i<ops; i++)
        script << oplist[insecure_rand() % (sizeof(oplist)/sizeof(oplist[0]))];
}

// Overwinter tx version numbers are selected randomly from current version range.
// http://en.cppreference.com/w/cpp/numeric/random/uniform_int_distribution
// https://stackoverflow.com/a/19728404
std::random_device rd;
std::mt19937 rng(rd());
std::uniform_int_distribution<int> overwinter_version_dist(
    CTransaction::OVERWINTER_MIN_CURRENT_VERSION,
    CTransaction::OVERWINTER_MAX_CURRENT_VERSION);
std::uniform_int_distribution<int> sapling_version_dist(
    CTransaction::SAPLING_MIN_CURRENT_VERSION,
    CTransaction::SAPLING_MAX_CURRENT_VERSION);

void static RandomTransaction(CMutableTransaction &tx, bool fSingle, uint32_t consensusBranchId) {
    tx.fOverwintered = insecure_rand() % 2;
    if (tx.fOverwintered) {
        if (insecure_rand() % 2) {
            tx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
            tx.nVersion = sapling_version_dist(rng);
        } else {
            tx.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID;
            tx.nVersion = overwinter_version_dist(rng);
        }
        tx.nExpiryHeight = (insecure_rand() % 2) ? insecure_rand() : 0;
    } else {
        tx.nVersion = insecure_rand() & 0x7FFFFFFF;
    }
    tx.vin.clear();
    tx.vout.clear();
    tx.vShieldedSpend.clear();
    tx.vShieldedOutput.clear();
    tx.vjoinsplit.clear();
    tx.nLockTime = (insecure_rand() % 2) ? insecure_rand() : 0;
    int ins = (insecure_rand() % 4) + 1;
    int outs = fSingle ? ins : (insecure_rand() % 4) + 1;
    int shielded_spends = (insecure_rand() % 4) + 1;
    int shielded_outs = (insecure_rand() % 4) + 1;
    int joinsplits = (insecure_rand() % 4);
    for (int in = 0; in < ins; in++) {
        tx.vin.push_back(CTxIn());
        CTxIn &txin = tx.vin.back();
        txin.prevout.hash = GetRandHash();
        txin.prevout.n = insecure_rand() % 4;
        RandomScript(txin.scriptSig);
        txin.nSequence = (insecure_rand() % 2) ? insecure_rand() : (unsigned int)-1;
    }
    for (int out = 0; out < outs; out++) {
        tx.vout.push_back(CTxOut());
        CTxOut &txout = tx.vout.back();
        txout.nValue = insecure_rand() % 100000000;
        RandomScript(txout.scriptPubKey);
    }
    if (tx.nVersionGroupId == SAPLING_VERSION_GROUP_ID) {
        tx.valueBalance = insecure_rand() % 100000000;
        for (int spend = 0; spend < shielded_spends; spend++) {
            SpendDescription sdesc;
            sdesc.cv = GetRandHash();
            sdesc.anchor = GetRandHash();
            sdesc.nullifier = GetRandHash();
            sdesc.rk = GetRandHash();
            randombytes_buf(sdesc.zkproof.begin(), sdesc.zkproof.size());
            tx.vShieldedSpend.push_back(sdesc);
        }
        for (int out = 0; out < shielded_outs; out++) {
            OutputDescription odesc;
            odesc.cv = GetRandHash();
            odesc.cm = GetRandHash();
            odesc.ephemeralKey = GetRandHash();
            randombytes_buf(odesc.encCiphertext.begin(), odesc.encCiphertext.size());
            randombytes_buf(odesc.outCiphertext.begin(), odesc.outCiphertext.size());
            randombytes_buf(odesc.zkproof.begin(), odesc.zkproof.size());
            tx.vShieldedOutput.push_back(odesc);
        }
    }
    if (tx.nVersion >= 2) {
        for (int js = 0; js < joinsplits; js++) {
            JSDescription jsdesc;
            if (insecure_rand() % 2 == 0) {
                jsdesc.vpub_old = insecure_rand() % 100000000;
            } else {
                jsdesc.vpub_new = insecure_rand() % 100000000;
            }

            jsdesc.anchor = GetRandHash();
            jsdesc.nullifiers[0] = GetRandHash();
            jsdesc.nullifiers[1] = GetRandHash();
            jsdesc.ephemeralKey = GetRandHash();
            jsdesc.randomSeed = GetRandHash();
            randombytes_buf(jsdesc.ciphertexts[0].begin(), jsdesc.ciphertexts[0].size());
            randombytes_buf(jsdesc.ciphertexts[1].begin(), jsdesc.ciphertexts[1].size());
            if (tx.fOverwintered && tx.nVersion >= SAPLING_TX_VERSION) {
                libzcash::GrothProof zkproof;
                randombytes_buf(zkproof.begin(), zkproof.size());
                jsdesc.proof = zkproof;
            } else {
                jsdesc.proof = libzcash::PHGRProof::random_invalid();
            }
            jsdesc.macs[0] = GetRandHash();
            jsdesc.macs[1] = GetRandHash();

            tx.vjoinsplit.push_back(jsdesc);
        }

        unsigned char joinSplitPrivKey[crypto_sign_SECRETKEYBYTES];
        crypto_sign_keypair(tx.joinSplitPubKey.begin(), joinSplitPrivKey);

        // Empty output script.
        CScript scriptCode;
        CTransaction signTx(tx);
        uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId);

        assert(crypto_sign_detached(&tx.joinSplitSig[0], NULL,
                                    dataToBeSigned.begin(), 32,
                                    joinSplitPrivKey
                                    ) == 0);
    }
}

BOOST_FIXTURE_TEST_SUITE(sighash_tests, JoinSplitTestingSetup)

BOOST_AUTO_TEST_CASE(sighash_test)
{
    uint32_t overwinterBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_OVERWINTER].nBranchId;
    seed_insecure_rand(false);

    #if defined(PRINT_SIGHASH_JSON)
    std::cout << "[\n";
    std::cout << "\t[\"raw_transaction, script, input_index, hashType, branchId, signature_hash (result)\"],\n";
    #endif
    int nRandomTests = 50000;

    #if defined(PRINT_SIGHASH_JSON)
    nRandomTests = 500;
    #endif
    for (int i=0; i<nRandomTests; i++) {
        int nHashType = insecure_rand();
        uint32_t consensusBranchId = NetworkUpgradeInfo[insecure_rand() % Consensus::MAX_NETWORK_UPGRADES].nBranchId;
        CMutableTransaction txTo;
        RandomTransaction(txTo, (nHashType & 0x1f) == SIGHASH_SINGLE, consensusBranchId);
        CScript scriptCode;
        RandomScript(scriptCode);
        int nIn = insecure_rand() % txTo.vin.size();

        uint256 sh, sho;
        sho = SignatureHashOld(scriptCode, txTo, nIn, nHashType);
        sh = SignatureHash(scriptCode, txTo, nIn, nHashType, 0, consensusBranchId);
        #if defined(PRINT_SIGHASH_JSON)
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << txTo;

        std::cout << "\t[\"" ;
        std::cout << HexStr(ss.begin(), ss.end()) << "\", \"";
        std::cout << HexStr(scriptCode) << "\", ";
        std::cout << nIn << ", ";
        std::cout << nHashType << ", ";
        std::cout << consensusBranchId << ", \"";
        std::cout << (txTo.fOverwintered ? sh.GetHex() : sho.GetHex()) << "\"]";
        if (i+1 != nRandomTests) {
          std::cout << ",";
        }
        std::cout << "\n";
        #endif
        if (!txTo.fOverwintered) {
            BOOST_CHECK(sh == sho);
        }
    }
