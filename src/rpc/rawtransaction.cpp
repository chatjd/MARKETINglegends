
// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/upgrades.h"
#include "base58.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "init.h"
#include "deprecation.h"
#include "key_io.h"
#include "keystore.h"
#include "main.h"
#include "merkleblock.h"
#include "net.h"
#include "primitives/transaction.h"
#include "rpc/server.h"
#include "script/script.h"
#include "script/script_error.h"
#include "script/sign.h"
#include "script/standard.h"
#include "uint256.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include <univalue.h>

using namespace std;

void ScriptPubKeyToJSON(const CScript& scriptPubKey, UniValue& out, bool fIncludeHex)
{
    txnouttype type;
    vector<CTxDestination> addresses;
    int nRequired;

    out.push_back(Pair("asm", ScriptToAsmStr(scriptPubKey)));
    if (fIncludeHex)
        out.push_back(Pair("hex", HexStr(scriptPubKey.begin(), scriptPubKey.end())));

    if (!ExtractDestinations(scriptPubKey, type, addresses, nRequired)) {
        out.push_back(Pair("type", GetTxnOutputType(type)));
        return;
    }

    out.push_back(Pair("reqSigs", nRequired));
    out.push_back(Pair("type", GetTxnOutputType(type)));

    UniValue a(UniValue::VARR);
    for (const CTxDestination& addr : addresses) {
        a.push_back(EncodeDestination(addr));
    }
    out.push_back(Pair("addresses", a));
}


UniValue TxJoinSplitToJSON(const CTransaction& tx) {
    bool useGroth = tx.fOverwintered && tx.nVersion >= SAPLING_TX_VERSION;
    UniValue vjoinsplit(UniValue::VARR);
    for (unsigned int i = 0; i < tx.vjoinsplit.size(); i++) {
        const JSDescription& jsdescription = tx.vjoinsplit[i];
        UniValue joinsplit(UniValue::VOBJ);

        joinsplit.push_back(Pair("vpub_old", ValueFromAmount(jsdescription.vpub_old)));
        joinsplit.push_back(Pair("vpub_oldZat", jsdescription.vpub_old));
        joinsplit.push_back(Pair("vpub_new", ValueFromAmount(jsdescription.vpub_new)));
        joinsplit.push_back(Pair("vpub_newZat", jsdescription.vpub_new));

        joinsplit.push_back(Pair("anchor", jsdescription.anchor.GetHex()));

        {
            UniValue nullifiers(UniValue::VARR);
            BOOST_FOREACH(const uint256 nf, jsdescription.nullifiers) {
                nullifiers.push_back(nf.GetHex());
            }
            joinsplit.push_back(Pair("nullifiers", nullifiers));
        }

        {
            UniValue commitments(UniValue::VARR);
            BOOST_FOREACH(const uint256 commitment, jsdescription.commitments) {
                commitments.push_back(commitment.GetHex());
            }
            joinsplit.push_back(Pair("commitments", commitments));
        }

        joinsplit.push_back(Pair("onetimePubKey", jsdescription.ephemeralKey.GetHex()));
        joinsplit.push_back(Pair("randomSeed", jsdescription.randomSeed.GetHex()));

        {
            UniValue macs(UniValue::VARR);
            BOOST_FOREACH(const uint256 mac, jsdescription.macs) {
                macs.push_back(mac.GetHex());
            }
            joinsplit.push_back(Pair("macs", macs));
        }

        CDataStream ssProof(SER_NETWORK, PROTOCOL_VERSION);
        auto ps = SproutProofSerializer<CDataStream>(ssProof, useGroth);
        boost::apply_visitor(ps, jsdescription.proof);
        joinsplit.push_back(Pair("proof", HexStr(ssProof.begin(), ssProof.end())));

        {
            UniValue ciphertexts(UniValue::VARR);
            for (const ZCNoteEncryption::Ciphertext ct : jsdescription.ciphertexts) {
                ciphertexts.push_back(HexStr(ct.begin(), ct.end()));
            }
            joinsplit.push_back(Pair("ciphertexts", ciphertexts));
        }

        vjoinsplit.push_back(joinsplit);
    }
    return vjoinsplit;
}

UniValue TxShieldedSpendsToJSON(const CTransaction& tx) {
    UniValue vdesc(UniValue::VARR);
    for (const SpendDescription& spendDesc : tx.vShieldedSpend) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("cv", spendDesc.cv.GetHex()));
        obj.push_back(Pair("anchor", spendDesc.anchor.GetHex()));
        obj.push_back(Pair("nullifier", spendDesc.nullifier.GetHex()));
        obj.push_back(Pair("rk", spendDesc.rk.GetHex()));
        obj.push_back(Pair("proof", HexStr(spendDesc.zkproof.begin(), spendDesc.zkproof.end())));
        obj.push_back(Pair("spendAuthSig", HexStr(spendDesc.spendAuthSig.begin(), spendDesc.spendAuthSig.end())));
        vdesc.push_back(obj);
    }
    return vdesc;
}

UniValue TxShieldedOutputsToJSON(const CTransaction& tx) {
    UniValue vdesc(UniValue::VARR);
    for (const OutputDescription& outputDesc : tx.vShieldedOutput) {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("cv", outputDesc.cv.GetHex()));
        obj.push_back(Pair("cmu", outputDesc.cm.GetHex()));
        obj.push_back(Pair("ephemeralKey", outputDesc.ephemeralKey.GetHex()));
        obj.push_back(Pair("encCiphertext", HexStr(outputDesc.encCiphertext.begin(), outputDesc.encCiphertext.end())));
        obj.push_back(Pair("outCiphertext", HexStr(outputDesc.outCiphertext.begin(), outputDesc.outCiphertext.end())));
        obj.push_back(Pair("proof", HexStr(outputDesc.zkproof.begin(), outputDesc.zkproof.end())));
        vdesc.push_back(obj);
    }
    return vdesc;
}

void TxToJSONExpanded(const CTransaction& tx, const uint256 hashBlock, UniValue& entry,
                      int nHeight = 0, int nConfirmations = 0, int nBlockTime = 0)
{
    uint256 txid = tx.GetHash();
    entry.push_back(Pair("txid", txid.GetHex()));
    entry.push_back(Pair("overwintered", tx.fOverwintered));
    entry.push_back(Pair("version", tx.nVersion));
    if (tx.fOverwintered) {
        entry.push_back(Pair("versiongroupid", HexInt(tx.nVersionGroupId)));
    }
    entry.push_back(Pair("locktime", (int64_t)tx.nLockTime));
    if (tx.fOverwintered) {
        entry.push_back(Pair("expiryheight", (int64_t)tx.nExpiryHeight));
    }
    UniValue vin(UniValue::VARR);
    BOOST_FOREACH(const CTxIn& txin, tx.vin) {
        UniValue in(UniValue::VOBJ);