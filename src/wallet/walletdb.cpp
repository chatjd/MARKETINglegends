// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/walletdb.h"

#include "consensus/validation.h"
#include "key_io.h"
#include "main.h"
#include "protocol.h"
#include "serialize.h"
#include "sync.h"
#include "util.h"
#include "utiltime.h"
#include "wallet/wallet.h"
#include "vidulum/Proof.hpp"

#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>

using namespace std;

static uint64_t nAccountingEntryNumber = 0;

//
// CWalletDB
//

bool CWalletDB::WriteName(const string& strAddress, const string& strName)
{
    nWalletDBUpdated++;
    return Write(make_pair(string("name"), strAddress), strName);
}

bool CWalletDB::EraseName(const string& strAddress)
{
    // This should only be used for sending addresses, never for receiving addresses,
    // receiving addresses must always have an address book entry if they're not change return.
    nWalletDBUpdated++;
    return Erase(make_pair(string("name"), strAddress));
}

bool CWalletDB::WritePurpose(const string& strAddress, const string& strPurpose)
{
    nWalletDBUpdated++;
    return Write(make_pair(string("purpose"), strAddress), strPurpose);
}

bool CWalletDB::ErasePurpose(const string& strPurpose)
{
    nWalletDBUpdated++;
    return Erase(make_pair(string("purpose"), strPurpose));
}

bool CWalletDB::WriteTx(uint256 hash, const CWalletTx& wtx)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("tx"), hash), wtx);
}

bool CWalletDB::EraseTx(uint256 hash)
{
    nWalletDBUpdated++;
    return Erase(std::make_pair(std::string("tx"), hash));
}

bool CWalletDB::WriteKey(const CPubKey& vchPubKey, const CPrivKey& vchPrivKey, const CKeyMetadata& keyMeta)
{
    nWalletDBUpdated++;

    if (!Write(std::make_pair(std::string("keymeta"), vchPubKey),
               keyMeta, false))
        return false;

    // hash pubkey/privkey to accelerate wallet load
    std::vector<unsigned char> vchKey;
    vchKey.reserve(vchPubKey.size() + vchPrivKey.size());
    vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
    vchKey.insert(vchKey.end(), vchPrivKey.begin(), vchPrivKey.end());

    return Write(std::make_pair(std::string("key"), vchPubKey), std::make_pair(vchPrivKey, Hash(vchKey.begin(), vchKey.end())), false);
}

bool CWalletDB::WriteCryptedKey(const CPubKey& vchPubKey,
                                const std::vector<unsigned char>& vchCryptedSecret,
                                const CKeyMetadata &keyMeta)
{
    const bool fEraseUnencryptedKey = true;
    nWalletDBUpdated++;

    if (!Write(std::make_pair(std::string("keymeta"), vchPubKey),
            keyMeta))
        return false;

    if (!Write(std::make_pair(std::string("ckey"), vchPubKey), vchCryptedSecret, false))
        return false;
    if (fEraseUnencryptedKey)
    {
        Erase(std::make_pair(std::string("key"), vchPubKey));
        Erase(std::make_pair(std::string("wkey"), vchPubKey));
    }
    return true;
}

bool CWalletDB::WriteCryptedZKey(const libzcash::SproutPaymentAddress & addr,
                                 const libzcash::ReceivingKey &rk,
                                 const std::vector<unsigned char>& vchCryptedSecret,
                                 const CKeyMetadata &keyMeta)
{
    const bool fEraseUnencryptedKey = true;
    nWalletDBUpdated++;

    if (!Write(std::make_pair(std::string("zkeymeta"), addr), keyMeta))
        return false;

    if (!Write(std::make_pair(std::string("czkey"), addr), std::make_pair(rk, vchCryptedSecret), false))
        return false;
    if (fEraseUnencryptedKey)
    {
        Erase(std::make_pair(std::string("zkey"), addr));
    }
    return true;
}

bool CWalletDB::WriteCryptedSaplingZKey(
    const libzcash::SaplingExtendedFullViewingKey &extfvk,
    const std::vector<unsigned char>& vchCryptedSecret,
    const CKeyMetadata &keyMeta)
{
    const bool fEraseUnencryptedKey = true;
    nWalletDBUpdated++;
    auto ivk = extfvk.fvk.in_viewing_key();

    if (!Write(std::make_pair(std::string("sapzkeymeta"), ivk), keyMeta))
        return false;

    if (!Write(std::make_pair(std::string("csapzkey"), ivk), std::make_pair(extfvk, vchCryptedSecret), false))
        return false;

    if (fEraseUnencryptedKey)
    {
        Erase(std::make_pair(std::string("sapzkey"), ivk));
    }
    return true;
}

bool CWalletDB::WriteMasterKey(unsigned int nID, const CMasterKey& kMasterKey)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("mkey"), nID), kMasterKey, true);
}

bool CWalletDB::WriteZKey(const libzcash::SproutPaymentAddress& addr, const libzcash::SproutSpendingKey& key, const CKeyMetadata &keyMeta)
{
    nWalletDBUpdated++;

    if (!Write(std::make_pair(std::string("zkeymeta"), addr), keyMeta))
        return false;

    // pair is: tuple_key("zkey", paymentaddress) --> secretkey
    return Write(std::make_pair(std::string("zkey"), addr), key, false);
}
bool CWalletDB::WriteSaplingZKey(const libzcash::SaplingIncomingViewingKey &ivk,
                const libzcash::SaplingExtendedSpendingKey &key,
                const CKeyMetadata &keyMeta)
{
    nWalletDBUpdated++;

    if (!Write(std::make_pair(std::string("sapzkeymeta"), ivk), keyMeta))
        return false;

    return Write(std::make_pair(std::string("sapzkey"), ivk), key, false);
}

bool CWalletDB::WriteSaplingPaymentAddress(
    const libzcash::SaplingPaymentAddress &addr,
    const libzcash::SaplingIncomingViewingKey &ivk)
{
    nWalletDBUpdated++;

    return Write(std::make_pair(std::string("sapzaddr"), addr), ivk, false);
}

bool CWalletDB::WriteSproutViewingKey(const libzcash::SproutViewingKey &vk)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("vkey"), vk), '1');
}

bool CWalletDB::EraseSproutViewingKey(const libzcash::SproutViewingKey &vk)
{
    nWalletDBUpdated++;
    return Erase(std::make_pair(std::string("vkey"), vk));
}

bool CWalletDB::WriteCScript(const uint160& hash, const CScript& redeemScript)
{
    nWalletDBUpdated++;
    return Write(std::make_pair(std::string("cscript"), hash), *(const CScriptBase