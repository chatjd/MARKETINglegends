// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/wallet.h"

#include "checkpoints.h"
#include "coincontrol.h"
#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "consensus/consensus.h"
#include "init.h"
#include "key_io.h"
#include "main.h"
#include "masternode-budget.h"
#include "net.h"
#include "rpc/protocol.h"
#include "script/script.h"
#include "script/sign.h"
#include "spork.h"
#include "swifttx.h"
#include "timedata.h"
#include "utilmoneystr.h"
#include "vidulum/Note.hpp"
#include "crypter.h"
#include "vidulum/zip32.h"

#include <assert.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

using namespace std;
using namespace libzcash;

/**
 * Settings
 */
CFeeRate payTxFee(DEFAULT_TRANSACTION_FEE);
CAmount maxTxFee = DEFAULT_TRANSACTION_MAXFEE;
unsigned int nTxConfirmTarget = DEFAULT_TX_CONFIRM_TARGET;
bool bSpendZeroConfChange = true;
bool fSendFreeTransactions = false;
bool fPayAtLeastCustomFee = true;

/**
 * Fees smaller than this (in satoshi) are considered zero fee (for transaction creation)
 * Override with -mintxfee
 */
CFeeRate CWallet::minTxFee = CFeeRate(1000);

/** @defgroup mapWallet
 *
 * @{
 */

struct CompareValueOnly
{
    bool operator()(const pair<CAmount, pair<const CWalletTx*, unsigned int> >& t1,
                    const pair<CAmount, pair<const CWalletTx*, unsigned int> >& t2) const
    {
        return t1.first < t2.first;
    }
};

std::string JSOutPoint::ToString() const
{
    return strprintf("JSOutPoint(%s, %d, %d)", hash.ToString().substr(0,10), js, n);
}

std::string COutput::ToString() const
{
    return strprintf("COutput(%s, %d, %d) [%s]", tx->GetHash().ToString(), i, nDepth, FormatMoney(tx->vout[i].nValue));
}

const CWalletTx* CWallet::GetWalletTx(const uint256& hash) const
{
    LOCK(cs_wallet);
    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(hash);
    if (it == mapWallet.end())
        return NULL;
    return &(it->second);
}

// Generate a new spending key and return its public payment address
libzcash::SproutPaymentAddress CWallet::GenerateNewSproutZKey()
{
    AssertLockHeld(cs_wallet); // mapSproutZKeyMetadata

    auto k = SproutSpendingKey::random();
    auto addr = k.address();

    // Check for collision, even though it is unlikely to ever occur
    if (CCryptoKeyStore::HaveSproutSpendingKey(addr))
        throw std::runtime_error("CWallet::GenerateNewSproutZKey(): Collision detected");

    // Create new metadata
    int64_t nCreationTime = GetTime();
    mapSproutZKeyMetadata[addr] = CKeyMetadata(nCreationTime);

    if (!AddSproutZKey(k))
        throw std::runtime_error("CWallet::GenerateNewSproutZKey(): AddSproutZKey failed");
    return addr;
}

// Generate a new Sapling spending key and return its public payment address
SaplingPaymentAddress CWallet::GenerateNewSaplingZKey()
{
    AssertLockHeld(cs_wallet); // mapSaplingZKeyMetadata

    // Create new metadata
    int64_t nCreationTime = GetTime();
    CKeyMetadata metadata(nCreationTime);

    // Try to get the seed
    HDSeed seed;
    if (!GetHDSeed(seed))
        throw std::runtime_error("CWallet::GenerateNewSaplingZKey(): HD seed not found");

    auto m = libzcash::SaplingExtendedSpendingKey::Master(seed);
    uint32_t bip44CoinType = Params().BIP44CoinType();

    // We use a fixed keypath scheme of m/32'/coin_type'/account'
    // Derive m/32'
    auto m_32h = m.Derive(32 | ZIP32_HARDENED_KEY_LIMIT);
    // Derive m/32'/coin_type'
    auto m_32h_cth = m_32h.Derive(bip44CoinType | ZIP32_HARDENED_KEY_LIMIT);

    // Derive account key at next index, skip keys already known to the wallet
    libzcash::SaplingExtendedSpendingKey xsk;
    do
    {
        xsk = m_32h_cth.Derive(hdChain.saplingAccountCounter | ZIP32_HARDENED_KEY_LIMIT);
        metadata.hdKeypath = "m/32'/" + std::to_string(bip44CoinType) + "'/" + std::to_string(hdChain.saplingAccountCounter) + "'";
        metadata.seedFp = hdChain.seedFp;
        // Increment childkey index
        hdChain.saplingAccountCounter++;
    } while (HaveSaplingSpendingKey(xsk.expsk.full_viewing_key()));

    // Update the chain model in the database
    if (fFileBacked && !CWalletDB(strWalletFile).WriteHDChain(hdChain))
        throw std::runtime_error("CWallet::GenerateNewSaplingZKey(): Writing HD chain model failed");

    auto ivk = xsk.expsk.full_viewing_key().in_viewing_key();
    mapSaplingZKeyMetadata[ivk] = metadata;

    auto addr = xsk.DefaultAddress();
    if (!AddSaplingZKey(xsk, addr)) {
        throw std::runtime_error("CWallet::GenerateNewSaplingZKey(): AddSaplingZKey failed");
    }
    // return default sapling payment address.
    return addr;
}

// Add spending key to keystore 
bool CWallet::AddSaplingZKey(
    const libzcash::SaplingExtendedSpendingKey &sk,
    const libzcash::SaplingPaymentAddress &defaultAddr)
{
    AssertLockHeld(cs_wallet); // mapSaplingZKeyMetadata

    if (!CCryptoKeyStore::AddSaplingSpendingKey(sk, defaultAddr)) {
        return false;
    }
    
    if (!fFileBacked) {
        return true;
    }

    if (!IsCrypted()) {
        auto ivk = sk.expsk.full_viewing_key().in_viewing_key();
        return CWalletDB(strWalletFile).WriteSaplingZKey(ivk, sk, mapSaplingZKeyMetadata[ivk]);
    }
    
    return true;
}

// Add payment address -> incoming viewing key map entry
bool CWallet::AddSaplingIncomingViewingKey(
    const libzcash::SaplingIncomingViewingKey &ivk,
    const libzcash::SaplingPaymentAddress &addr)
{
    AssertLockHeld(cs_wallet); // mapSaplingZKeyMetadata

    if (!CCryptoKeyStore::AddSaplingIncomingViewingKey(ivk, addr)) {
        return false;
    }

    if (!fFileBacked) {
        return true;
    }

    if (!IsCrypted()) {
        return CWalletDB(strWalletFile).WriteSaplingPaymentAddress(addr, ivk);
    }

    return true;
}


// Add spending key to keystore and persist to disk
bool CWallet::AddSproutZKey(const libzcash::SproutSpendingKey &key)
{
    AssertLockHeld(cs_wallet); // mapSproutZKeyMetadata
    auto addr = key.address();

    if (!CCryptoKeyStore::AddSproutSpendingKey(key))
        return false;

    // check if we need to remove from viewing keys
    if (HaveSproutViewingKey(addr))
        RemoveSproutViewingKey(key.viewing_key());

    if (!fFileBacked)
        return true;

    if (!IsCrypted()) {
        return CWalletDB(strWalletFile).WriteZKey(addr,
                                                  key,
                                                  mapSproutZKeyMetadata[addr]);
    }
    return true;
}

CPubKey CWallet::GenerateNewKey()
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    bool fCompressed = CanSupportFeature(FEATURE_COMPRPUBKEY); // default to compressed public keys if we want 0.6.0 wallets

    CKey secret;
    secret.MakeNewKey(fCompressed);

    // Compressed public keys were introduced in version 0.6.0
    if (fCompressed)
        SetMinVersion(FEATURE_COMPRPUBKEY);

    CPubKey pubkey = secret.GetPubKey();
    assert(secret.VerifyPubKey(pubkey));

    // Create new metadata
    int64_t nCreationTime = GetTime();
    mapKeyMetadata[pubkey.GetID()] = CKeyMetadata(nCreationTime);
    if (!nTimeFirstKey || nCreationTime < nTimeFirstKey)
        nTimeFirstKey = nCreationTime;

    if (!AddKeyPubKey(secret, pubkey))
        throw std::runtime_error("CWallet::GenerateNewKey(): AddKey failed");
    return pubkey;
}

bool CWallet::AddKeyPubKey(const CKey& secret, const CPubKey &pubkey)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (!CCryptoKeyStore::AddKeyPubKey(secret, pubkey))
        return false;

    // check if we need to remove from watch-only
    CScript script;
    script = GetScriptForDestination(pubkey.GetID());
    if (HaveWatchOnly(script))
        RemoveWatchOnly(script);

    if (!fFileBacked)
        return true;
    if (!IsCrypted()) {
        return CWalletDB(strWalletFile).WriteKey(pubkey,
                                                 secret.GetPrivKey(),
                                                 mapKeyMetadata[pubkey.GetID()]);
    }
    return true;
}

bool CWallet::AddCryptedKey(const CPubKey &vchPubKey,
                            const vector<unsigned char> &vchCryptedSecret)
{

    if (!CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret))
        return false;
    if (!fFileBacked)
        return true;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption)
            return pwalletdbEncryption->WriteCryptedKey(vchPubKey,
                                                        vchCryptedSecret,
                                                        mapKeyMetadata[vchPubKey.GetID()]);
        else
            return CWalletDB(strWalletFile).WriteCryptedKey(vchPubKey,
                                                            vchCryptedSecret,
                                                            mapKeyMetadata[vchPubKey.GetID()]);
    }
    return false;
}


bool CWallet::AddCryptedSproutSpendingKey(
    const libzcash::SproutPaymentAddress &address,
    const libzcash::ReceivingKey &rk,
    const std::vector<unsigned char> &vchCryptedSecret)
{
    if (!CCryptoKeyStore::AddCryptedSproutSpendingKey(address, rk, vchCryptedSecret))
        return false;
    if (!fFileBacked)
        return true;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption) {
            return pwalletdbEncryption->WriteCryptedZKey(address,
                                                         rk,
                                                         vchCryptedSecret,
                                                         mapSproutZKeyMetadata[address]);
        } else {
            return CWalletDB(strWalletFile).WriteCryptedZKey(address,
                                                             rk,
                                                             vchCryptedSecret,
                                                             mapSproutZKeyMetadata[address]);
        }
    }
    return false;
}

bool CWallet::AddCryptedSaplingSpendingKey(const libzcash::SaplingExtendedFullViewingKey &extfvk,
                                           const std::vector<unsigned char> &vchCryptedSecret,
                                           const libzcash::SaplingPaymentAddress &defaultAddr)
{
    if (!CCryptoKeyStore::AddCryptedSaplingSpendingKey(extfvk, vchCryptedSecret, defaultAddr))
        return false;
    if (!fFileBacked)
        return true;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption) {
            return pwalletdbEncryption->WriteCryptedSaplingZKey(extfvk,
                                                         vchCryptedSecret,
                                                         mapSaplingZKeyMetadata[extfvk.fvk.in_viewing_key()]);
        } else {
            return CWalletDB(strWalletFile).WriteCryptedSaplingZKey(extfvk,
                                                         vchCryptedSecret,
                                                         mapSaplingZKeyMetadata[extfvk.fvk.in_viewing_key()]);
        }
    }
    return false;
}

bool CWallet::LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &meta)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (meta.nCreateTime && (!nTimeFirstKey || meta.nCreateTime < nTimeFirstKey))
        nTimeFirstKey = meta.nCreateTime;

    mapKeyMetadata[pubkey.GetID()] = meta;
    return true;
}

bool CWallet::LoadZKeyMetadata(const SproutPaymentAddress &addr, const CKeyMetadata &meta)
{
    AssertLockHeld(cs_wallet); // mapSproutZKeyMetadata
    mapSproutZKeyMetadata[addr] = meta;
    return true;
}

bool CWallet::LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    return CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret);
}

bool CWallet::LoadCryptedZKey(const libzcash::SproutPaymentAddress &addr, const libzcash::ReceivingKey &rk, const std::vector<unsigned char> &vchCryptedSecret)
{
    return CCryptoKeyStore::AddCryptedSproutSpendingKey(addr, rk, vchCryptedSecret);
}

bool CWallet::LoadCryptedSaplingZKey(
    const libzcash::SaplingExtendedFullViewingKey &extfvk,
    const std::vector<unsigned char> &vchCryptedSecret)
{
     return CCryptoKeyStore::AddCryptedSaplingSpendingKey(extfvk, vchCryptedSecret, extfvk.DefaultAddress());
}

bool CWallet::LoadSaplingZKeyMetadata(const libzcash::SaplingIncomingViewingKey &ivk, const CKeyMetadata &meta)
{
    AssertLockHeld(cs_wallet); // mapSaplingZKeyMetadata
    mapSaplingZKeyMetadata[ivk] = meta;
    return true;
}

bool CWallet::LoadSaplingZKey(const libzcash::SaplingExtendedSpendingKey &key)
{
    return CCryptoKeyStore::AddSaplingSpendingKey(key, key.DefaultAddress());
}

bool CWallet::LoadSaplingPaymentAddress(
    const libzcash::SaplingPaymentAddress &addr,
    const libzcash::SaplingIncomingViewingKey &ivk)
{
    return CCryptoKeyStore::AddSaplingIncomingViewingKey(ivk, addr);
}

bool CWallet::LoadZKey(const libzcash::SproutSpendingKey &key)
{
    return CCryptoKeyStore::AddSproutSpendingKey(key);
}

bool CWallet::AddSproutViewingKey(const libzcash::SproutViewingKey &vk)
{
    if (!CCryptoKeyStore::AddSproutViewingKey(vk)) {
        return false;
    }
    nTimeFirstKey = 1; // No birthday information for viewing keys.
    if (!fFileBacked) {
        return true;
    }
    return CWalletDB(strWalletFile).WriteSproutViewingKey(vk);
}

bool CWallet::RemoveSproutViewingKey(const libzcash::SproutViewingKey &vk)
{
    AssertLockHeld(cs_wallet);
    if (!CCryptoKeyStore::RemoveSproutViewingKey(vk)) {
        return false;
    }
    if (fFileBacked) {
        if (!CWalletDB(strWalletFile).EraseSproutViewingKey(vk)) {
            return false;
        }
    }

    return true;
}

bool CWallet::LoadSproutViewingKey(const libzcash::SproutViewingKey &vk)
{
    return CCryptoKeyStore::AddSproutViewingKey(vk);
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    if (!CCryptoKeyStore::AddCScript(redeemScript))
        return false;
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteCScript(Hash160(redeemScript), redeemScript);
}

bool CWallet::LoadCScript(const CScript& redeemScript)
{
    /* A sanity check was added in pull #3843 to avoid adding redeemScripts
     * that never can be redeemed. However, old wallets may still contain
     * these. Do not add them to the wallet and warn. */
    if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE)
    {
        std::string strAddr = EncodeDestination(CScriptID(redeemScript));
        LogPrintf("%s: Warning: This wallet contains a redeemScript of size %i which exceeds maximum size %i thus can never be redeemed. Do not use address %s.\n",
            __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr);
        return true;
    }

    return CCryptoKeyStore::AddCScript(redeemScript);
}

bool CWallet::AddWatchOnly(const CScript &dest)
{
    if (!CCryptoKeyStore::AddWatchOnly(dest))
        return false;
    nTimeFirstKey = 1; // No birthday information for watch-only keys.
    NotifyWatchonlyChanged(true);
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteWatchOnly(dest);
}

bool CWallet::RemoveWatchOnly(const CScript &dest)
{
    AssertLockHeld(cs_wallet);
    if (!CCryptoKeyStore::RemoveWatchOnly(dest))
        return false;
    if (!HaveWatchOnly())
        NotifyWatchonlyChanged(false);
    if (fFileBacked)
        if (!CWalletDB(strWalletFile).EraseWatchOnly(dest))
            return false;

    return true;
}

bool CWallet::LoadWatchOnly(const CScript &dest)
{
    return CCryptoKeyStore::AddWatchOnly(dest);
}

bool CWallet::Unlock(const SecureString& strWalletPassphrase)
{
    CCrypter crypter;
    CKeyingMaterial vMasterKey;

    {
        LOCK(cs_wallet);
        BOOST_FOREACH(const MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                continue; // try another master key
            if (CCryptoKeyStore::Unlock(vMasterKey))
                return true;
        }
    }
    return false;
}

bool CWallet::ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase)
{
    bool fWasLocked = IsLocked();

    {
        LOCK(cs_wallet);
        Lock();

        CCrypter crypter;
        CKeyingMaterial vMasterKey;
        BOOST_FOREACH(MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strOldWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (CCryptoKeyStore::Unlock(vMasterKey))
            {
                int64_t nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = pMasterKey.second.nDeriveIterations * (100 / ((double)(GetTimeMillis() - nStartTime)));

                nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = (pMasterKey.second.nDeriveIterations + pMasterKey.second.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

                if (pMasterKey.second.nDeriveIterations < 25000)
                    pMasterKey.second.nDeriveIterations = 25000;

                LogPrintf("Wallet passphrase changed to an nDeriveIterations of %i\n", pMasterKey.second.nDeriveIterations);

                if (!crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                    return false;
                if (!crypter.Encrypt(vMasterKey, pMasterKey.second.vchCryptedKey))
                    return false;
                CWalletDB(strWalletFile).WriteMasterKey(pMasterKey.first, pMasterKey.second);
                if (fWasLocked)
                    Lock();
                return true;
            }
        }
    }

    return false;
}

void CWallet::ChainTip(const CBlockIndex *pindex, 
                       const CBlock *pblock,
                       SproutMerkleTree sproutTree,
                       SaplingMerkleTree saplingTree, 
                       bool added)
{
    if (added) {
        IncrementNoteWitnesses(pindex, pblock, sproutTree, saplingTree);
    } else {
        DecrementNoteWitnesses(pindex);
    }
    UpdateSaplingNullifierNoteMapForBlock(pblock);
}

void CWallet::SetBestChain(const CBlockLocator& loc)
{
    CWalletDB walletdb(strWalletFile);
    SetBestChainINTERNAL(walletdb, loc);
}

std::set<std::pair<libzcash::PaymentAddress, uint256>> CWallet::GetNullifiersForAddresses(
        const std::set<libzcash::PaymentAddress> & addresses)
{
    std::set<std::pair<libzcash::PaymentAddress, uint256>> nullifierSet;
    // Sapling ivk -> list of addrs map
    // (There may be more than one diversified address for a given ivk.)
    std::map<libzcash::SaplingIncomingViewingKey, std::vector<libzcash::SaplingPaymentAddress>> ivkMap;
    for (const auto & addr : addresses) {
        auto saplingAddr = boost::get<libzcash::SaplingPaymentAddress>(&addr);
        if (saplingAddr != nullptr) {
            libzcash::SaplingIncomingViewingKey ivk;
            this->GetSaplingIncomingViewingKey(*saplingAddr, ivk);
            ivkMap[ivk].push_back(*saplingAddr);
        }
    }
    for (const auto & txPair : mapWallet) {
        // Sprout
        for (const auto & noteDataPair : txPair.second.mapSproutNoteData) {
            auto & noteData = noteDataPair.second;
            auto & nullifier = noteData.nullifier;
            auto & address = noteData.address;
            if (nullifier && addresses.count(address)) {
                nullifierSet.insert(std::make_pair(address, nullifier.get()));
            }
        }
        // Sapling
        for (const auto & noteDataPair : txPair.second.mapSaplingNoteData) {
            auto & noteData = noteDataPair.second;
            auto & nullifier = noteData.nullifier;
            auto & ivk = noteData.ivk;
            if (nullifier && ivkMap.count(ivk)) {
                for (const auto & addr : ivkMap[ivk]) {
                    nullifierSet.insert(std::make_pair(addr, nullifier.get()));
                }
            }
        }
    }
    return nullifierSet;
}

bool CWallet::IsNoteSproutChange(
        const std::set<std::pair<libzcash::PaymentAddress, uint256>> & nullifierSet,
        const PaymentAddress & address,
        const JSOutPoint & jsop)
{
    // A Note is marked as "change" if the address that received it
    // also spent Notes in the same transaction. This will catch,
    // for instance:
    // - Change created by spending fractions of Notes (because
    //   z_sendmany sends change to the originating z-address).
    // - "Chaining Notes" used to connect JoinSplits together.
    // - Notes created by consolidation transactions (e.g. using
    //   z_mergetoaddress).
    // - Notes sent from one address to itself.
    for (const JSDescription & jsd : mapWallet[jsop.hash].vjoinsplit) {
        for (const uint256 & nullifier : jsd.nullifiers) {
            if (nullifierSet.count(std::make_pair(address, nullifier))) {
                return true;
            }
        }
    }
    return false;
}

bool CWallet::IsNoteSaplingChange(const std::set<std::pair<libzcash::PaymentAddress, uint256>> & nullifierSet,
        const libzcash::PaymentAddress & address,
        const SaplingOutPoint & op)
{
    // A Note is marked as "change" if the address that received it
    // also spent Notes in the same transaction. This will catch,
    // for instance:
    // - Change created by spending fractions of Notes (because
    //   z_sendmany sends change to the originating z-address).
    // - Notes created by consolidation transactions (e.g. using
    //   z_mergetoaddress).
    // - Notes sent from one address to itself.
    for (const SpendDescription &spend : mapWallet[op.hash].vShieldedSpend) {
        if (nullifierSet.count(std::make_pair(address, spend.nullifier))) {
            return true;
        }
    }
    return false;
}

bool CWallet::SetMinVersion(enum WalletFeature nVersion, CWalletDB* pwalletdbIn, bool fExplicit)
{
    LOCK(cs_wallet); // nWalletVersion
    if (nWalletVersion >= nVersion)
        return true;

    // when doing an explicit upgrade, if we pass the max version permitted, upgrade all the way
    if (fExplicit && nVersion > nWalletMaxVersion)
            nVersion = FEATURE_LATEST;

    nWalletVersion = nVersion;

    if (nVersion > nWalletMaxVersion)
        nWalletMaxVersion = nVersion;

    if (fFileBacked)
    {
        CWalletDB* pwalletdb = pwalletdbIn ? pwalletdbIn : new CWalletDB(strWalletFile);
        if (nWalletVersion > 40000)
            pwalletdb->WriteMinVersion(nWalletVersion);
        if (!pwalletdbIn)
            delete pwalletdb;
    }

    return true;
}

bool CWallet::SetMaxVersion(int nVersion)
{
    LOCK(cs_wallet); // nWalletVersion, nWalletMaxVersion
    // cannot downgrade below current version
    if (nWalletVersion > nVersion)
        return false;

    nWalletMaxVersion = nVersion;

    return true;
}

set<uint256> CWallet::GetConflicts(const uint256& txid) const
{
    set<uint256> result;
    AssertLockHeld(cs_wallet);

    std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(txid);
    if (it == mapWallet.end())
        return result;
    const CWalletTx& wtx = it->second;

    std::pair<TxSpends::const_iterator, TxSpends::const_iterator> range;

    BOOST_FOREACH(const CTxIn& txin, wtx.vin)
    {
        if (mapTxSpends.count(txin.prevout) <= 1)
            continue;  // No conflict if zero or one spends
        range = mapTxSpends.equal_range(txin.prevout);
        for (TxSpends::const_iterator it = range.first; it != range.second; ++it)
            result.insert(it->second);
    }

    std::pair<TxNullifiers::const_iterator, TxNullifiers::const_iterator> range_n;

    for (const JSDescription& jsdesc : wtx.vjoinsplit) {
        for (const uint256& nullifier : jsdesc.nullifiers) {
            if (mapTxSproutNullifiers.count(nullifier) <= 1) {
                continue;  // No conflict if zero or one spends
            }
            range_n = mapTxSproutNullifiers.equal_range(nullifier);
            for (TxNullifiers::const_iterator it = range_n.first; it != range_n.second; ++it) {
                result.insert(it->second);
            }
        }
    }

    std::pair<TxNullifiers::const_iterator, TxNullifiers::const_iterator> range_o;

    for (const SpendDescription &spend : wtx.vShieldedSpend) {
        uint256 nullifier = spend.nullifier;
        if (mapTxSaplingNullifiers.count(nullifier) <= 1) {
            continue;  // No conflict if zero or one spends
        }
        range_o = mapTxSaplingNullifiers.equal_range(nullifier);
        for (TxNullifiers::const_iterator it = range_o.first; it != range_o.second; ++it) {
            result.insert(it->second);
        }
    }
    return result;
}

void CWallet::Flush(bool shutdown)
{
    bitdb.Flush(shutdown);
}

bool CWallet::Verify(const string& walletFile, string& warningString, string& errorString)
{
    if (!bitdb.Open(GetDataDir()))
    {
        // try moving the database env out of the way
        boost::filesystem::path pathDatabase = GetDataDir() / "database";
        boost::filesystem::path pathDatabaseBak = GetDataDir() / strprintf("database.%d.bak", GetTime());
        try {
            boost::filesystem::rename(pathDatabase, pathDatabaseBak);
            LogPrintf("Moved old %s to %s. Retrying.\n", pathDatabase.string(), pathDatabaseBak.string());
        } catch (const boost::filesystem::filesystem_error&) {
            // failure is ok (well, not really, but it's not worse than what we started with)
        }

        // try again
        if (!bitdb.Open(GetDataDir())) {
            // if it still fails, it probably means we can't even create the database env
            string msg = strprintf(_("Error initializing wallet database environment %s!"), GetDataDir());
            errorString += msg;
            return true;
        }
    }

    if (GetBoolArg("-salvagewallet", false))
    {
        // Recover readable keypairs:
        if (!CWalletDB::Recover(bitdb, walletFile, true))
            return false;
    }

    if (boost::filesystem::exists(GetDataDir() / walletFile))
    {
        CDBEnv::VerifyResult r = bitdb.Verify(walletFile, CWalletDB::Recover);
        if (r == CDBEnv::RECOVER_OK)
        {
            warningString += strprintf(_("Warning: wallet.dat corrupt, data salvaged!"
                                     " Original wallet.dat saved as wallet.{timestamp}.bak in %s; if"
                                     " your balance or transactions are incorrect you should"
                                     " restore from a backup."), GetDataDir());
        }
        if (r == CDBEnv::RECOVER_FAIL)
            errorString += _("wallet.dat corrupt, salvage failed");
    }

    return true;
}

template <class T>
void CWallet::SyncMetaData(pair<typename TxSpendMap<T>::iterator, typename TxSpendMap<T>::iterator> range)
{
    // We want all the wallet transactions in range to have the same metadata as
    // the oldest (smallest nOrderPos).
    // So: find smallest nOrderPos:

    int nMinOrderPos = std::numeric_limits<int>::max();
    const CWalletTx* copyFrom = NULL;
    for (typename TxSpendMap<T>::iterator it = range.first; it != range.second; ++it)
    {
        const uint256& hash = it->second;
        int n = mapWallet[hash].nOrderPos;
        if (n < nMinOrderPos)
        {
            nMinOrderPos = n;
            copyFrom = &mapWallet[hash];
        }
    }
    // Now copy data from copyFrom to rest:
    for (typename TxSpendMap<T>::iterator it = range.first; it != range.second; ++it)
    {
        const uint256& hash = it->second;
        CWalletTx* copyTo = &mapWallet[hash];
        if (copyFrom == copyTo) continue;
        copyTo->mapValue = copyFrom->mapValue;
        // mapSproutNoteData and mapSaplingNoteData not copied on purpose
        // (it is always set correctly for each CWalletTx)
        copyTo->vOrderForm = copyFrom->vOrderForm;
        // fTimeReceivedIsTxTime not copied on purpose
        // nTimeReceived not copied on purpose
        copyTo->nTimeSmart = copyFrom->nTimeSmart;
        copyTo->fFromMe = copyFrom->fFromMe;
        copyTo->strFromAccount = copyFrom->strFromAccount;
        // nOrderPos not copied on purpose
        // cached members not copied on purpose
    }
}

/**
 * Outpoint is spent if any non-conflicted transaction
 * spends it:
 */
bool CWallet::IsSpent(const uint256& hash, unsigned int n) const
{
    const COutPoint outpoint(hash, n);
    pair<TxSpends::const_iterator, TxSpends::const_iterator> range;
    range = mapTxSpends.equal_range(outpoint);

    for (TxSpends::const_iterator it = range.first; it != range.second; ++it)
    {
        const uint256& wtxid = it->second;
        std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(wtxid);
        if (mit != mapWallet.end() && mit->second.GetDepthInMainChain() >= 0)
            return true; // Spent
    }
    return false;
}

/**
 * Note is spent if any non-conflicted transaction
 * spends it:
 */
bool CWallet::IsSproutSpent(const uint256& nullifier) const {
    pair<TxNullifiers::const_iterator, TxNullifiers::const_iterator> range;
    range = mapTxSproutNullifiers.equal_range(nullifier);

    for (TxNullifiers::const_iterator it = range.first; it != range.second; ++it) {
        const uint256& wtxid = it->second;
        std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(wtxid);
        if (mit != mapWallet.end() && mit->second.GetDepthInMainChain() >= 0) {
            return true; // Spent
        }
    }
    return false;
}

bool CWallet::IsSaplingSpent(const uint256& nullifier) const {
    pair<TxNullifiers::const_iterator, TxNullifiers::const_iterator> range;
    range = mapTxSaplingNullifiers.equal_range(nullifier);

    for (TxNullifiers::const_iterator it = range.first; it != range.second; ++it) {
        const uint256& wtxid = it->second;
        std::map<uint256, CWalletTx>::const_iterator mit = mapWallet.find(wtxid);
        if (mit != mapWallet.end() && mit->second.GetDepthInMainChain() >= 0) {
            return true; // Spent
        }
    }
    return false;
}

void CWallet::AddToTransparentSpends(const COutPoint& outpoint, const uint256& wtxid)
{
    mapTxSpends.insert(make_pair(outpoint, wtxid));

    pair<TxSpends::iterator, TxSpends::iterator> range;
    range = mapTxSpends.equal_range(outpoint);
    SyncMetaData<COutPoint>(range);
}

void CWallet::AddToSproutSpends(const uint256& nullifier, const uint256& wtxid)
{
    mapTxSproutNullifiers.insert(make_pair(nullifier, wtxid));

    pair<TxNullifiers::iterator, TxNullifiers::iterator> range;
    range = mapTxSproutNullifiers.equal_range(nullifier);
    SyncMetaData<uint256>(range);
}

void CWallet::AddToSaplingSpends(const uint256& nullifier, const uint256& wtxid)
{
    mapTxSaplingNullifiers.insert(make_pair(nullifier, wtxid));

    pair<TxNullifiers::iterator, TxNullifiers::iterator> range;
    range = mapTxSaplingNullifiers.equal_range(nullifier);
    SyncMetaData<uint256>(range);
}

void CWallet::AddToSpends(const uint256& wtxid)
{
    assert(mapWallet.count(wtxid));
    CWalletTx& thisTx = mapWallet[wtxid];
    if (thisTx.IsCoinBase()) // Coinbases don't spend anything!
        return;

    for (const CTxIn& txin : thisTx.vin) {
        AddToTransparentSpends(txin.prevout, wtxid);
    }
    for (const JSDescription& jsdesc : thisTx.vjoinsplit) {
        for (const uint256& nullifier : jsdesc.nullifiers) {
            AddToSproutSpends(nullifier, wtxid);
        }
    }
    for (const SpendDescription &spend : thisTx.vShieldedSpend) {
        AddToSaplingSpends(spend.nullifier, wtxid);
    }
}

bool CWallet::GetMasternodeVinAndKeys(CTxIn& txinRet, CPubKey& pubKeyRet, CKey& keyRet, std::string strTxHash, std::string strOutputIndex)
{
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    // Find possible candidates
    std::vector<COutput> vPossibleCoins;

    AvailableCoins(vPossibleCoins, true, NULL, false, true, ONLY_15000);
    if (vPossibleCoins.empty()) {
        LogPrintf("CWallet::GetMasternodeVinAndKeys -- Could not locate any valid masternode vin\n");
        return false;
    }

     LogPrintf("strTxHash = %s\n", strTxHash.c_str());
    if (strTxHash.empty()) // No output specified, select the first one
        return GetVinAndKeysFromOutput(vPossibleCoins[0], txinRet, pubKeyRet, keyRet);

    // Find specific vin
    uint256 txHash = uint256S(strTxHash);

    int nOutputIndex;
    try {
        nOutputIndex = std::stoi(strOutputIndex.c_str());
    } catch (const std::exception& e) {
        LogPrintf("%s: %s on strOutputIndex\n", __func__, e.what());
        return false;
    }

    BOOST_FOREACH (COutput& out, vPossibleCoins)
        if (out.tx->GetHash() == txHash && out.i == nOutputIndex) // found it!
            return GetVinAndKeysFromOutput(out, txinRet, pubKeyRet, keyRet);

    LogPrintf("CWallet::GetMasternodeVinAndKeys -- Could not locate specified masternode vin\n");
    return false;
}

bool CWallet::GetVinAndKeysFromOutput(COutput out, CTxIn& txinRet, CPubKey& pubKeyRet, CKey& keyRet)
{
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    CScript pubScript;

    txinRet = CTxIn(out.tx->GetHash(), out.i);
    pubScript = out.tx->vout[out.i].scriptPubKey; // the inputs PubKey

    CTxDestination address1;
    ExtractDestination(pubScript, address1);

    const CKeyID *keyID = boost::get<CKeyID>(&address1);
    if (!keyID) {
        LogPrintf("CWallet::GetVinAndKeysFromOutput -- Address does not refer to a key\n");
        return false;
    }

    if (!GetKey(*keyID, keyRet)) {
        LogPrintf("CWallet::GetVinAndKeysFromOutput -- Private key for address is not known\n");
        return false;
    }

    pubKeyRet = keyRet.GetPubKey();
    return true;
}
void CWallet::ClearNoteWitnessCache()
{
    LOCK(cs_wallet);
    for (std::pair<const uint256, CWalletTx>& wtxItem : mapWallet) {
        for (mapSproutNoteData_t::value_type& item : wtxItem.second.mapSproutNoteData) {
            item.second.witnesses.clear();
            item.second.witnessHeight = -1;
        }
        for (mapSaplingNoteData_t::value_type& item : wtxItem.second.mapSaplingNoteData) {
            item.second.witnesses.clear();
            item.second.witnessHeight = -1;
        }
    }
    nWitnessCacheSize = 0;
}

template<typename NoteDataMap>
void CopyPreviousWitnesses(NoteDataMap& noteDataMap, int indexHeight, int64_t nWitnessCacheSize)
{
    for (auto& item : noteDataMap) {
        auto* nd = &(item.second);
        // Only increment witnesses that are behind the current height
        if (nd->witnessHeight < indexHeight) {
            // Check the validity of the cache
            // The only time a note witnessed above the current height
            // would be invalid here is during a reindex when blocks
            // have been decremented, and we are incrementing the blocks
            // immediately after.
            assert(nWitnessCacheSize >= nd->witnesses.size());
            // Witnesses being incremented should always be either -1
            // (never incremented or decremented) or one below indexHeight
            assert((nd->witnessHeight == -1) || (nd->witnessHeight == indexHeight - 1));
            // Copy the witness for the previous block if we have one
            if (nd->witnesses.size() > 0) {
                nd->witnesses.push_front(nd->witnesses.front());
            }
            if (nd->witnesses.size() > WITNESS_CACHE_SIZE) {
                nd->witnesses.pop_back();
            }
        }
    }
}

template<typename NoteDataMap>
void AppendNoteCommitment(NoteDataMap& noteDataMap, int indexHeight, int64_t nWitnessCacheSize, const uint256& note_commitment)
{
    for (auto& item : noteDataMap) {
        auto* nd = &(item.second);
        if (nd->witnessHeight < indexHeight && nd->witnesses.size() > 0) {
            // Check the validity of the cache
            // See comment in CopyPreviousWitnesses about validity.
            assert(nWitnessCacheSize >= nd->witnesses.size());
            nd->witnesses.front().append(note_commitment);
        }
    }
}

template<typename OutPoint, typename NoteData, typename Witness>
void WitnessNoteIfMine(std::map<OutPoint, NoteData>& noteDataMap, int indexHeight, int64_t nWitnessCacheSize, const OutPoint& key, const Witness& witness)
{
    if (noteDataMap.count(key) && noteDataMap[key].witnessHeight < indexHeight) {
        auto* nd = &(noteDataMap[key]);
        if (nd->witnesses.size() > 0) {
            // We think this can happen because we write out the
            // witness cache state after every block increment or
            // decrement, but the block index itself is written in
            // batches. So if the node crashes in between these two
            // operations, it is possible for IncrementNoteWitnesses
            // to be called again on previously-cached blocks. This
            // doesn't affect existing cached notes because of the
            // NoteData::witnessHeight checks. See #1378 for details.
            LogPrintf("Inconsistent witness cache state found for %s\n- Cache size: %d\n- Top (height %d): %s\n- New (height %d): %s\n",
                        key.ToString(), nd->witnesses.size(),
                        nd->witnessHeight,
                        nd->witnesses.front().root().GetHex(),
                        indexHeight,
                        witness.root().GetHex());
            nd->witnesses.clear();
        }
        nd->witnesses.push_front(witness);
        // Set height to one less than pindex so it gets incremented
        nd->witnessHeight = indexHeight - 1;
        // Check the validity of the cache
        assert(nWitnessCacheSize >= nd->witnesses.size());
    }
}


template<typename NoteDataMap>
void UpdateWitnessHeights(NoteDataMap& noteDataMap, int indexHeight, int64_t nWitnessCacheSize)
{
    for (auto& item : noteDataMap) {
        auto* nd = &(item.second);
        if (nd->witnessHeight < indexHeight) {
            nd->witnessHeight = indexHeight;
            // Check the validity of the cache
            // See comment in CopyPreviousWitnesses about validity.
            assert(nWitnessCacheSize >= nd->witnesses.size());
        }
    }
}

void CWallet::IncrementNoteWitnesses(const CBlockIndex* pindex,
                                     const CBlock* pblockIn,
                                     SproutMerkleTree& sproutTree,
                                     SaplingMerkleTree& saplingTree)
{
    LOCK(cs_wallet);
    for (std::pair<const uint256, CWalletTx>& wtxItem : mapWallet) {
       ::CopyPreviousWitnesses(wtxItem.second.mapSproutNoteData, pindex->nHeight, nWitnessCacheSize);
       ::CopyPreviousWitnesses(wtxItem.second.mapSaplingNoteData, pindex->nHeight, nWitnessCacheSize);
    }

    if (nWitnessCacheSize < WITNESS_CACHE_SIZE) {
        nWitnessCacheSize += 1;
    }

    const CBlock* pblock {pblockIn};
    CBlock block;
    if (!pblock) {
        ReadBlockFromDisk(block, pindex);
        pblock = &block;
    }

    for (const CTransaction& tx : pblock->vtx) {
        auto hash = tx.GetHash();
        bool txIsOurs = mapWallet.count(hash);
        // Sprout
        for (size_t i = 0; i < tx.vjoinsplit.size(); i++) {
            const JSDescription& jsdesc = tx.vjoinsplit[i];
            for (uint8_t j = 0; j < jsdesc.commitments.size(); j++) {
                const uint256& note_commitment = jsdesc.commitments[j];
                sproutTree.append(note_commitment);

                // Increment existing witnesses
                for (std::pair<const uint256, CWalletTx>& wtxItem : mapWallet) {
                    ::AppendNoteCommitment(wtxItem.second.mapSproutNoteData, pindex->nHeight, nWitnessCacheSize, note_commitment);
                }

                // If this is our note, witness it
                if (txIsOurs) {
                    JSOutPoint jsoutpt {hash, i, j};
                    ::WitnessNoteIfMine(mapWallet[hash].mapSproutNoteData, pindex->nHeight, nWitnessCacheSize, jsoutpt, sproutTree.witness());
                }
            }
        }
        // Sapling
        for (uint32_t i = 0; i < tx.vShieldedOutput.size(); i++) {
            const uint256& note_commitment = tx.vShieldedOutput[i].cm;
            saplingTree.append(note_commitment);

            // Increment existing witnesses
            for (std::pair<const uint256, CWalletTx>& wtxItem : mapWallet) {
                ::AppendNoteCommitment(wtxItem.second.mapSaplingNoteData, pindex->nHeight, nWitnessCacheSize, note_commitment);
            }

            // If this is our note, witness it
            if (txIsOurs) {
                SaplingOutPoint outPoint {hash, i};
                ::WitnessNoteIfMine(mapWallet[hash].mapSaplingNoteData, pindex->nHeight, nWitnessCacheSize, outPoint, saplingTree.witness());
            }
        }
    }

    // Update witness heights
    for (std::pair<const uint256, CWalletTx>& wtxItem : mapWallet) {
        ::UpdateWitnessHeights(wtxItem.second.mapSproutNoteData, pindex->nHeight, nWitnessCacheSize);
        ::UpdateWitnessHeights(wtxItem.second.mapSaplingNoteData, pindex->nHeight, nWitnessCacheSize);
    }

    // For performance reasons, we write out the witness cache in
    // CWallet::SetBestChain() (which also ensures that overall consistency
    // of the wallet.dat is maintained).
}

template<typename NoteDataMap>
void DecrementNoteWitnesses(NoteDataMap& noteDataMap, int indexHeight, int64_t nWitnessCacheSize)
{
    for (auto& item : noteDataMap) {
        auto* nd = &(item.second);
        // Only decrement witnesses that are not above the current height
        if (nd->witnessHeight <= indexHeight) {
            // Check the validity of the cache
            // See comment below (this would be invalid if there were a
            // prior decrement).
            assert(nWitnessCacheSize >= nd->witnesses.size());
            // Witnesses being decremented should always be either -1
            // (never incremented or decremented) or equal to the height
            // of the block being removed (indexHeight)
            assert((nd->witnessHeight == -1) || (nd->witnessHeight == indexHeight));
            if (nd->witnesses.size() > 0) {
                nd->witnesses.pop_front();
            }
            // indexHeight is the height of the block being removed, so 
            // the new witness cache height is one below it.
            nd->witnessHeight = indexHeight - 1;
        }
        // Check the validity of the cache
        // Technically if there are notes witnessed above the current
        // height, their cache will now be invalid (relative to the new
        // value of nWitnessCacheSize). However, this would only occur
        // during a reindex, and by the time the reindex reaches the tip
        // of the chain again, the existing witness caches will be valid
        // again.
        // We don't set nWitnessCacheSize to zero at the start of the
        // reindex because the on-disk blocks had already resulted in a
        // chain that didn't trigger the assertion below.
        if (nd->witnessHeight < indexHeight) {
            // Subtract 1 to compare to what nWitnessCacheSize will be after
            // decrementing.
            assert((nWitnessCacheSize - 1) >= nd->witnesses.size());
        }
    }
}

void CWallet::DecrementNoteWitnesses(const CBlockIndex* pindex)
{
    LOCK(cs_wallet);
    for (std::pair<const uint256, CWalletTx>& wtxItem : mapWallet) {
        ::DecrementNoteWitnesses(wtxItem.second.mapSproutNoteData, pindex->nHeight, nWitnessCacheSize);
        ::DecrementNoteWitnesses(wtxItem.second.mapSaplingNoteData, pindex->nHeight, nWitnessCacheSize);
    }
    nWitnessCacheSize -= 1;
    // TODO: If nWitnessCache is zero, we need to regenerate the caches (#1302)
    assert(nWitnessCacheSize > 0);

    // For performance reasons, we write out the witness cache in
    // CWallet::SetBestChain() (which also ensures that overall consistency
    // of the wallet.dat is maintained).
}

bool CWallet::EncryptWallet(const SecureString& strWalletPassphrase)
{
    if (IsCrypted())
        return false;

    CKeyingMaterial vMasterKey;

    vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
    GetRandBytes(&vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

    CMasterKey kMasterKey;

    kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
    GetRandBytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

    CCrypter crypter;
    int64_t nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, 25000, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = 2500000 / ((double)(GetTimeMillis() - nStartTime));

    nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = (kMasterKey.nDeriveIterations + kMasterKey.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

    if (kMasterKey.nDeriveIterations < 25000)
        kMasterKey.nDeriveIterations = 25000;

    LogPrintf("Encrypting Wallet with an nDeriveIterations of %i\n", kMasterKey.nDeriveIterations);

    if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod))
        return false;
    if (!crypter.Encrypt(vMasterKey, kMasterKey.vchCryptedKey))
        return false;

    {
        LOCK(cs_wallet);
        mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
        if (fFileBacked)
        {
            assert(!pwalletdbEncryption);
            pwalletdbEncryption = new CWalletDB(strWalletFile);
            if (!pwalletdbEncryption->TxnBegin()) {
                delete pwalletdbEncryption;
                pwalletdbEncryption = NULL;
                return false;
            }
            pwalletdbEncryption->WriteMasterKey(nMasterKeyMaxID, kMasterKey);
        }

        if (!EncryptKeys(vMasterKey))
        {
            if (fFileBacked) {
                pwalletdbEncryption->TxnAbort();
                delete pwalletdbEncryption;
            }
            // We now probably have half of our keys encrypted in memory, and half not...
            // die and let the user reload the unencrypted wallet.
            assert(false);
        }

        // Encryption was introduced in version 0.4.0
        SetMinVersion(FEATURE_WALLETCRYPT, pwalletdbEncryption, true);

        if (fFileBacked)
        {
            if (!pwalletdbEncryption->TxnCommit()) {
                delete pwalletdbEncryption;
                // We now have keys encrypted in memory, but not on disk...
                // die to avoid confusion and let the user reload the unencrypted wallet.
                assert(false);
            }

            delete pwalletdbEncryption;
            pwalletdbEncryption = NULL;
        }

        Lock();
        Unlock(strWalletPassphrase);
        NewKeyPool();
        Lock();

        // Need to completely rewrite the wallet file; if we don't, bdb might keep
        // bits of the unencrypted private key in slack space in the database file.
        CDB::Rewrite(strWalletFile);

    }
    NotifyStatusChanged(this);

    return true;
}

int64_t CWallet::IncOrderPosNext(CWalletDB *pwalletdb)
{
    AssertLockHeld(cs_wallet); // nOrderPosNext
    int64_t nRet = nOrderPosNext++;
    if (pwalletdb) {
        pwalletdb->WriteOrderPosNext(nOrderPosNext);
    } else {
        CWalletDB(strWalletFile).WriteOrderPosNext(nOrderPosNext);
    }
    return nRet;
}

CWallet::TxItems CWallet::OrderedTxItems(std::list<CAccountingEntry>& acentries, std::string strAccount)
{
    AssertLockHeld(cs_wallet); // mapWallet
    CWalletDB walletdb(strWalletFile);

    // First: get all CWalletTx and CAccountingEntry into a sorted-by-order multimap.
    TxItems txOrdered;

    // Note: maintaining indices in the database of (account,time) --> txid and (account, time) --> acentry
    // would make this much faster for applications that do this a lot.
    for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        CWalletTx* wtx = &((*it).second);
        txOrdered.insert(make_pair(wtx->nOrderPos, TxPair(wtx, (CAccountingEntry*)0)));
    }
    acentries.clear();
    walletdb.ListAccountCreditDebit(strAccount, acentries);
    BOOST_FOREACH(CAccountingEntry& entry, acentries)
    {
        txOrdered.insert(make_pair(entry.nOrderPos, TxPair((CWalletTx*)0, &entry)));
    }

    return txOrdered;
}

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
            item.second.MarkDirty();
    }
}

/**
 * Ensure that every note in the wallet (for which we possess a spending key)
 * has a cached nullifier.
 */
bool CWallet::UpdateNullifierNoteMap()
{
    {
        LOCK(cs_wallet);

        if (IsLocked())
            return false;

        ZCNoteDecryption dec;
        for (std::pair<const uint256, CWalletTx>& wtxItem : mapWallet) {
            for (mapSproutNoteData_t::value_type& item : wtxItem.second.mapSproutNoteData) {
                if (!item.second.nullifier) {
                    if (GetNoteDecryptor(item.second.address, dec)) {
                        auto i = item.first.js;
                        auto hSig = wtxItem.second.vjoinsplit[i].h_sig(
                            *pvidulumParams, wtxItem.second.joinSplitPubKey);
                        item.second.nullifier = GetSproutNoteNullifier(
                            wtxItem.second.vjoinsplit[i],
                            item.second.address,
                            dec,
                            hSig,
                            item.first.n);
                    }
                }
            }

            // TODO: Sapling.  This method is only called from RPC walletpassphrase, which is currently unsupported
            // as RPC encryptwallet is hidden behind two flags: -developerencryptwallet -experimentalfeatures

            UpdateNullifierNoteMapWithTx(wtxItem.second);
        }
    }
    return true;
}

/**
 * Update mapSproutNullifiersToNotes and mapSaplingNullifiersToNotes
 * with the cached nullifiers in this tx.
 */
void CWallet::UpdateNullifierNoteMapWithTx(const CWalletTx& wtx)
{
    {
        LOCK(cs_wallet);
        for (const mapSproutNoteData_t::value_type& item : wtx.mapSproutNoteData) {
            if (item.second.nullifier) {
                mapSproutNullifiersToNotes[*item.second.nullifier] = item.first;
            }
        }

        for (const mapSaplingNoteData_t::value_type& item : wtx.mapSaplingNoteData) {
            if (item.second.nullifier) {
                mapSaplingNullifiersToNotes[*item.second.nullifier] = item.first;
            }
        }
    }
}

/**
 * Update mapSaplingNullifiersToNotes, computing the nullifier from a cached witness if necessary.
 */
void CWallet::UpdateSaplingNullifierNoteMapWithTx(CWalletTx& wtx) {
    LOCK(cs_wallet);

    for (mapSaplingNoteData_t::value_type &item : wtx.mapSaplingNoteData) {
        SaplingOutPoint op = item.first;
        SaplingNoteData nd = item.second;

        if (nd.witnesses.empty()) {
            // If there are no witnesses, erase the nullifier and associated mapping.
            if (item.second.nullifier) {
                mapSaplingNullifiersToNotes.erase(item.second.nullifier.get());
            }
            item.second.nullifier = boost::none;
        }
        else {
            uint64_t position = nd.witnesses.front().position();
            SaplingFullViewingKey fvk = mapSaplingFullViewingKeys.at(nd.ivk);
            OutputDescription output = wtx.vShieldedOutput[op.n];
            auto optPlaintext = SaplingNotePlaintext::decrypt(output.encCiphertext, nd.ivk, output.ephemeralKey, output.cm);
            if (!optPlaintext) {
                // An item in mapSaplingNoteData must have already been successfully decrypted,
                // otherwise the item would not exist in the first place.
                assert(false);
            }
            auto optNote = optPlaintext.get().note(nd.ivk);
            if (!optNote) {
                assert(false);
            }
            auto optNullifier = optNote.get().nullifier(fvk, position);
            if (!optNullifier) {
                // This should not happen.  If it does, maybe the position has been corrupted or miscalculated?
                assert(false);
            }
            uint256 nullifier = optNullifier.get();
            mapSaplingNullifiersToNotes[nullifier] = op;
            item.second.nullifier = nullifier;
        }
    }
}

/**
 * Iterate over transactions in a block and update the cached Sapling nullifiers
 * for transactions which belong to the wallet.
 */
void CWallet::UpdateSaplingNullifierNoteMapForBlock(const CBlock *pblock) {
    LOCK(cs_wallet);

    for (const CTransaction& tx : pblock->vtx) {
        auto hash = tx.GetHash();
        bool txIsOurs = mapWallet.count(hash);
        if (txIsOurs) {
            UpdateSaplingNullifierNoteMapWithTx(mapWallet[hash]);
        }
    }
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn, bool fFromLoadWallet, CWalletDB* pwalletdb)
{
    uint256 hash = wtxIn.GetHash();

    if (fFromLoadWallet)
    {
        mapWallet[hash] = wtxIn;
        mapWallet[hash].BindWallet(this);
        UpdateNullifierNoteMapWithTx(mapWallet[hash]);
        AddToSpends(hash);
    }
    else
    {
        LOCK(cs_wallet);
        // Inserts only if not already there, returns tx inserted or tx found
        pair<map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(make_pair(hash, wtxIn));
        CWalletTx& wtx = (*ret.first).second;
        wtx.BindWallet(this);
        UpdateNullifierNoteMapWithTx(wtx);
        bool fInsertedNew = ret.second;
        if (fInsertedNew)
        {
            wtx.nTimeReceived = GetAdjustedTime();
            wtx.nOrderPos = IncOrderPosNext(pwalletdb);

            wtx.nTimeSmart = wtx.nTimeReceived;
            if (!wtxIn.hashBlock.IsNull())
            {
                if (mapBlockIndex.count(wtxIn.hashBlock))
                {
                    int64_t latestNow = wtx.nTimeReceived;
                    int64_t latestEntry = 0;
                    {
                        // Tolerate times up to the last timestamp in the wallet not more than 5 minutes into the future
                        int64_t latestTolerated = latestNow + 300;
                        std::list<CAccountingEntry> acentries;
                        TxItems txOrdered = OrderedTxItems(acentries);
                        for (TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
                        {
                            CWalletTx *const pwtx = (*it).second.first;
                            if (pwtx == &wtx)
                                continue;
                            CAccountingEntry *const pacentry = (*it).second.second;
                            int64_t nSmartTime;
                            if (pwtx)
                            {
                                nSmartTime = pwtx->nTimeSmart;
                                if (!nSmartTime)
                                    nSmartTime = pwtx->nTimeReceived;
                            }
                            else
                                nSmartTime = pacentry->nTime;
                            if (nSmartTime <= latestTolerated)
                            {
                                latestEntry = nSmartTime;
                                if (nSmartTime > latestNow)
                                    latestNow = nSmartTime;
                                break;
                            }
                        }
                    }

                    int64_t blocktime = mapBlockIndex[wtxIn.hashBlock]->GetBlockTime();
                    wtx.nTimeSmart = std::max(latestEntry, std::min(blocktime, latestNow));
                }
                else
                    LogPrintf("AddToWallet(): found %s in block %s not in index\n",
                             wtxIn.GetHash().ToString(),
                             wtxIn.hashBlock.ToString());
            }
            AddToSpends(hash);
        }

        bool fUpdated = false;
        if (!fInsertedNew)
        {
            // Merge
            if (!wtxIn.hashBlock.IsNull() && wtxIn.hashBlock != wtx.hashBlock)
            {
                wtx.hashBlock = wtxIn.hashBlock;
                fUpdated = true;
            }
            if (wtxIn.nIndex != -1 && (wtxIn.vMerkleBranch != wtx.vMerkleBranch || wtxIn.nIndex != wtx.nIndex))
            {
                wtx.vMerkleBranch = wtxIn.vMerkleBranch;
                wtx.nIndex = wtxIn.nIndex;
                fUpdated = true;
            }
            if (UpdatedNoteData(wtxIn, wtx)) {
                fUpdated = true;
            }
            if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe)
            {
                wtx.fFromMe = wtxIn.fFromMe;
                fUpdated = true;
            }
        }

        //// debug print
        LogPrintf("AddToWallet %s  %s%s\n", wtxIn.GetHash().ToString(), (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

        // Write to disk
        if (fInsertedNew || fUpdated)
            if (!wtx.WriteToDisk(pwalletdb))
                return false;

        // Break debit/credit balance caches:
        wtx.MarkDirty();

        // Notify UI of new or updated transaction
        NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);

        // notify an external script when a wallet transaction comes in or is updated
        std::string strCmd = GetArg("-walletnotify", "");

        if ( !strCmd.empty())
        {
            boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
            boost::thread t(runCommand, strCmd); // thread runs free
        }

    }
    return true;
}

bool CWallet::UpdatedNoteData(const CWalletTx& wtxIn, CWalletTx& wtx)
{
    bool unchangedSproutFlag = (wtxIn.mapSproutNoteData.empty() || wtxIn.mapSproutNoteData == wtx.mapSproutNoteData);
    if (!unchangedSproutFlag) {
        auto tmp = wtxIn.mapSproutNoteData;
        // Ensure we keep any cached witnesses we may already have
        for (const std::pair <JSOutPoint, SproutNoteData> nd : wtx.mapSproutNoteData) {
            if (tmp.count(nd.first) && nd.second.witnesses.size() > 0) {
                tmp.at(nd.first).witnesses.assign(
                        nd.second.witnesses.cbegin(), nd.second.witnesses.cend());
            }
            tmp.at(nd.first).witnessHeight = nd.second.witnessHeight;
        }
        // Now copy over the updated note data
        wtx.mapSproutNoteData = tmp;
    }

    bool unchangedSaplingFlag = (wtxIn.mapSaplingNoteData.empty() || wtxIn.mapSaplingNoteData == wtx.mapSaplingNoteData);
    if (!unchangedSaplingFlag) {
        auto tmp = wtxIn.mapSaplingNoteData;
        // Ensure we keep any cached witnesses we may already have

        for (const std::pair <SaplingOutPoint, SaplingNoteData> nd : wtx.mapSaplingNoteData) {
            if (tmp.count(nd.first) && nd.second.witnesses.size() > 0) {
                tmp.at(nd.first).witnesses.assign(
                        nd.second.witnesses.cbegin(), nd.second.witnesses.cend());
            }
            tmp.at(nd.first).witnessHeight = nd.second.witnessHeight;
        }

        // Now copy over the updated note data
        wtx.mapSaplingNoteData = tmp;
    }

    return !unchangedSproutFlag || !unchangedSaplingFlag;
}

/**
 * Add a transaction to the wallet, or update it.
 * pblock is optional, but should be provided if the transaction is known to be in a block.
 * If fUpdate is true, existing transactions will be updated.
 */
bool CWallet::AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate)
{
    {
        AssertLockHeld(cs_wallet);
        bool fExisted = mapWallet.count(tx.GetHash()) != 0;
        if (fExisted && !fUpdate) return false;
        auto sproutNoteData = FindMySproutNotes(tx);
        auto saplingNoteDataAndAddressesToAdd = FindMySaplingNotes(tx);
        auto saplingNoteData = saplingNoteDataAndAddressesToAdd.first;
        auto addressesToAdd = saplingNoteDataAndAddressesToAdd.second;
        for (const auto &addressToAdd : addressesToAdd) {
            if (!AddSaplingIncomingViewingKey(addressToAdd.second, addressToAdd.first)) {
                return false;
            }
        }
        if (fExisted || IsMine(tx) || IsFromMe(tx) || sproutNoteData.size() > 0 || saplingNoteData.size() > 0)
        {
            CWalletTx wtx(this,tx);

            if (sproutNoteData.size() > 0) {
                wtx.SetSproutNoteData(sproutNoteData);
            }

            if (saplingNoteData.size() > 0) {
                wtx.SetSaplingNoteData(saplingNoteData);
            }

            // Get merkle branch if transaction was found in a block
            if (pblock)
                wtx.SetMerkleBranch(*pblock);

            // Do not flush the wallet here for performance reasons
            // this is safe, as in case of a crash, we rescan the necessary blocks on startup through our SetBestChain-mechanism
            CWalletDB walletdb(strWalletFile, "r+", false);

            return AddToWallet(wtx, false, &walletdb);
        }
    }
    return false;
}

void CWallet::SyncTransaction(const CTransaction& tx, const CBlock* pblock)
{
    LOCK2(cs_main, cs_wallet);
    if (!AddToWalletIfInvolvingMe(tx, pblock, true))
        return; // Not one of ours

    MarkAffectedTransactionsDirty(tx);
}

void CWallet::MarkAffectedTransactionsDirty(const CTransaction& tx)
{
    // If a transaction changes 'conflicted' state, that changes the balance
    // available of the outputs it spends. So force those to be
    // recomputed, also:
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        if (mapWallet.count(txin.prevout.hash))
            mapWallet[txin.prevout.hash].MarkDirty();
    }
    for (const JSDescription& jsdesc : tx.vjoinsplit) {
        for (const uint256& nullifier : jsdesc.nullifiers) {
            if (mapSproutNullifiersToNotes.count(nullifier) &&
                mapWallet.count(mapSproutNullifiersToNotes[nullifier].hash)) {
                mapWallet[mapSproutNullifiersToNotes[nullifier].hash].MarkDirty();
            }
        }
    }

    for (const SpendDescription &spend : tx.vShieldedSpend) {
        uint256 nullifier = spend.nullifier;
        if (mapSaplingNullifiersToNotes.count(nullifier) &&
            mapWallet.count(mapSaplingNullifiersToNotes[nullifier].hash)) {
            mapWallet[mapSaplingNullifiersToNotes[nullifier].hash].MarkDirty();
        }
    }
}

void CWallet::EraseFromWallet(const uint256 &hash)
{
    if (!fFileBacked)
        return;
    {
        LOCK(cs_wallet);
        if (mapWallet.erase(hash))
            CWalletDB(strWalletFile).EraseTx(hash);
    }
    return;
}


/**
 * Returns a nullifier if the SpendingKey is available
 * Throws std::runtime_error if the decryptor doesn't match this note
 */
boost::optional<uint256> CWallet::GetSproutNoteNullifier(const JSDescription &jsdesc,
                                                         const libzcash::SproutPaymentAddress &address,
                                                         const ZCNoteDecryption &dec,
                                                         const uint256 &hSig,
                                                         uint8_t n) const
{
    boost::optional<uint256> ret;
    auto note_pt = libzcash::SproutNotePlaintext::decrypt(
        dec,
        jsdesc.ciphertexts[n],
        jsdesc.ephemeralKey,
        hSig,
        (unsigned char) n);
    auto note = note_pt.note(address);
    // SpendingKeys are only available if:
    // - We have them (this isn't a viewing key)
    // - The wallet is unlocked
    libzcash::SproutSpendingKey key;
    if (GetSproutSpendingKey(address, key)) {
        ret = note.nullifier(key);
    }
    return ret;
}

/**
 * Finds all output notes in the given transaction that have been sent to
 * PaymentAddresses in this wallet.
 *
 * It should never be necessary to call this method with a CWalletTx, because
 * the result of FindMySproutNotes (for the addresses available at the time) will
 * already have been cached in CWalletTx.mapSproutNoteData.
 */
mapSproutNoteData_t CWallet::FindMySproutNotes(const CTransaction &tx) const
{
    LOCK(cs_SpendingKeyStore);
    uint256 hash = tx.GetHash();

    mapSproutNoteData_t noteData;
    for (size_t i = 0; i < tx.vjoinsplit.size(); i++) {
        auto hSig = tx.vjoinsplit[i].h_sig(*pvidulumParams, tx.joinSplitPubKey);
        for (uint8_t j = 0; j < tx.vjoinsplit[i].ciphertexts.size(); j++) {
            for (const NoteDecryptorMap::value_type& item : mapNoteDecryptors) {
                try {
                    auto address = item.first;
                    JSOutPoint jsoutpt {hash, i, j};
                    auto nullifier = GetSproutNoteNullifier(
                        tx.vjoinsplit[i],
                        address,
                        item.second,
                        hSig, j);
                    if (nullifier) {
                        SproutNoteData nd {address, *nullifier};
                        noteData.insert(std::make_pair(jsoutpt, nd));
                    } else {
                        SproutNoteData nd {address};
                        noteData.insert(std::make_pair(jsoutpt, nd));
                    }
                    break;
                } catch (const note_decryption_failed &err) {
                    // Couldn't decrypt with this decryptor
                } catch (const std::exception &exc) {
                    // Unexpected failure
                    LogPrintf("FindMySproutNotes(): Unexpected error while testing decrypt:\n");
                    LogPrintf("%s\n", exc.what());
                }
            }
        }
    }
    return noteData;
}


/**
 * Finds all output notes in the given transaction that have been sent to
 * SaplingPaymentAddresses in this wallet.
 *
 * It should never be necessary to call this method with a CWalletTx, because
 * the result of FindMySaplingNotes (for the addresses available at the time) will
 * already have been cached in CWalletTx.mapSaplingNoteData.
 */
std::pair<mapSaplingNoteData_t, SaplingIncomingViewingKeyMap> CWallet::FindMySaplingNotes(const CTransaction &tx) const
{
    LOCK(cs_SpendingKeyStore);
    uint256 hash = tx.GetHash();

    mapSaplingNoteData_t noteData;
    SaplingIncomingViewingKeyMap viewingKeysToAdd;

    // Protocol Spec: 4.19 Block Chain Scanning (Sapling)
    for (uint32_t i = 0; i < tx.vShieldedOutput.size(); ++i) {
        const OutputDescription output = tx.vShieldedOutput[i];
        for (auto it = mapSaplingFullViewingKeys.begin(); it != mapSaplingFullViewingKeys.end(); ++it) {
            SaplingIncomingViewingKey ivk = it->first;
            auto result = SaplingNotePlaintext::decrypt(output.encCiphertext, ivk, output.ephemeralKey, output.cm);
            if (!result) {
                continue;
            }
            auto address = ivk.address(result.get().d);
            if (address && mapSaplingIncomingViewingKeys.count(address.get()) == 0) {
                viewingKeysToAdd[address.get()] = ivk;
            }
            // We don't cache the nullifier here as computing it requires knowledge of the note position
            // in the commitment tree, which can only be determined when the transaction has been mined.
            SaplingOutPoint op {hash, i};
            SaplingNoteData nd;
            nd.ivk = ivk;
            noteData.insert(std::make_pair(op, nd));
            break;
        }
    }

    return std::make_pair(noteData, viewingKeysToAdd);
}

bool CWallet::IsSproutNullifierFromMe(const uint256& nullifier) const
{
    {
        LOCK(cs_wallet);
        if (mapSproutNullifiersToNotes.count(nullifier) &&
                mapWallet.count(mapSproutNullifiersToNotes.at(nullifier).hash)) {
            return true;
        }
    }
    return false;
}

bool CWallet::IsSaplingNullifierFromMe(const uint256& nullifier) const
{
    {
        LOCK(cs_wallet);
        if (mapSaplingNullifiersToNotes.count(nullifier) &&
                mapWallet.count(mapSaplingNullifiersToNotes.at(nullifier).hash)) {
            return true;
        }
    }
    return false;
}

void CWallet::GetSproutNoteWitnesses(std::vector<JSOutPoint> notes,
                                     std::vector<boost::optional<SproutWitness>>& witnesses,
                                     uint256 &final_anchor)
{
    LOCK(cs_wallet);
    witnesses.resize(notes.size());
    boost::optional<uint256> rt;
    int i = 0;
    for (JSOutPoint note : notes) {
        if (mapWallet.count(note.hash) &&
                mapWallet[note.hash].mapSproutNoteData.count(note) &&
                mapWallet[note.hash].mapSproutNoteData[note].witnesses.size() > 0) {
            witnesses[i] = mapWallet[note.hash].mapSproutNoteData[note].witnesses.front();
            if (!rt) {
                rt = witnesses[i]->root();
            } else {
                assert(*rt == witnesses[i]->root());
            }
        }
        i++;
    }
    // All returned witnesses have the same anchor
    if (rt) {
        final_anchor = *rt;
    }
}

void CWallet::GetSaplingNoteWitnesses(std::vector<SaplingOutPoint> notes,
                                      std::vector<boost::optional<SaplingWitness>>& witnesses,
                                      uint256 &final_anchor)
{
    LOCK(cs_wallet);
    witnesses.resize(notes.size());
    boost::optional<uint256> rt;
    int i = 0;
    for (SaplingOutPoint note : notes) {
        if (mapWallet.count(note.hash) &&
                mapWallet[note.hash].mapSaplingNoteData.count(note) &&
                mapWallet[note.hash].mapSaplingNoteData[note].witnesses.size() > 0) {
            witnesses[i] = mapWallet[note.hash].mapSaplingNoteData[note].witnesses.front();
            if (!rt) {
                rt = witnesses[i]->root();
            } else {
                assert(*rt == witnesses[i]->root());
            }
        }
        i++;
    }
    // All returned witnesses have the same anchor
    if (rt) {
        final_anchor = *rt;
    }
}

isminetype CWallet::IsMine(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                return IsMine(prev.vout[txin.prevout.n]);
        }
    }
    return ISMINE_NO;
}

CAmount CWallet::GetDebit(const CTxIn &txin, const isminefilter& filter) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                if (IsMine(prev.vout[txin.prevout.n]) & filter)
                    return prev.vout[txin.prevout.n].nValue;
        }
    }
    return 0;
}

isminetype CWallet::IsMine(const CTxOut& txout) const
{
    return ::IsMine(*this, txout.scriptPubKey);
}

CAmount CWallet::GetCredit(const CTxOut& txout, const isminefilter& filter) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error("CWallet::GetCredit(): value out of range");
    return ((IsMine(txout) & filter) ? txout.nValue : 0);
}


// Recursively determine the rounds of a given input (How deep is the Obfuscation chain for a given input)
int CWallet::GetRealInputObfuscationRounds(CTxIn in, int rounds) const
{
    static std::map<uint256, CMutableTransaction> mDenomWtxes;

    if (rounds >= 16) return 15; // 16 rounds max

    uint256 hash = in.prevout.hash;
    unsigned int nout = in.prevout.n;

    const CWalletTx* wtx = GetWalletTx(hash);
    if (wtx != NULL) {
        std::map<uint256, CMutableTransaction>::const_iterator mdwi = mDenomWtxes.find(hash);
        // not known yet, let's add it
        if (mdwi == mDenomWtxes.end()) {
            LogPrint("obfuscation", "GetInputObfuscationRounds INSERTING %s\n", hash.ToString());
            mDenomWtxes[hash] = CMutableTransaction(*wtx);
        }
        // found and it's not an initial value, just return it
        else if (mDenomWtxes[hash].vout[nout].nRounds != -10) {
            return mDenomWtxes[hash].vout[nout].nRounds;
        }


        // bounds check
        if (nout >= wtx->vout.size()) {
            // should never actually hit this
            LogPrint("obfuscation", "GetInputObfuscationRounds UPDATED   %s %3d %3d\n", hash.ToString(), nout, -4);
            return -4;
        }

        if (pwalletMain->IsCollateralAmount(wtx->vout[nout].nValue)) {
            mDenomWtxes[hash].vout[nout].nRounds = -3;
            LogPrint("obfuscation", "GetInputObfuscationRounds UPDATED   %s %3d %3d\n", hash.ToString(), nout, mDenomWtxes[hash].vout[nout].nRounds);
            return mDenomWtxes[hash].vout[nout].nRounds;
        }

        //make sure the final output is non-denominate
        if (/*rounds == 0 && */ !IsDenominatedAmount(wtx->vout[nout].nValue)) //NOT DENOM
        {
            mDenomWtxes[hash].vout[nout].nRounds = -2;
            LogPrint("obfuscation", "GetInputObfuscationRounds UPDATED   %s %3d %3d\n", hash.ToString(), nout, mDenomWtxes[hash].vout[nout].nRounds);
            return mDenomWtxes[hash].vout[nout].nRounds;
        }

        bool fAllDenoms = true;
        BOOST_FOREACH (CTxOut out, wtx->vout) {
            fAllDenoms = fAllDenoms && IsDenominatedAmount(out.nValue);
        }
        // this one is denominated but there is another non-denominated output found in the same tx
        if (!fAllDenoms) {
            mDenomWtxes[hash].vout[nout].nRounds = 0;
            LogPrint("obfuscation", "GetInputObfuscationRounds UPDATED   %s %3d %3d\n", hash.ToString(), nout, mDenomWtxes[hash].vout[nout].nRounds);
            return mDenomWtxes[hash].vout[nout].nRounds;
        }

        int nShortest = -10; // an initial value, should be no way to get this by calculations
        bool fDenomFound = false;
        // only denoms here so let's look up
        BOOST_FOREACH (CTxIn in2, wtx->vin) {
            if (IsMine(in2)) {
                int n = GetRealInputObfuscationRounds(in2, rounds + 1);
                // denom found, find the shortest chain or initially assign nShortest with the first found value
                if (n >= 0 && (n < nShortest || nShortest == -10)) {
                    nShortest = n;
                    fDenomFound = true;
                }
            }
        }
        mDenomWtxes[hash].vout[nout].nRounds = fDenomFound ? (nShortest >= 15 ? 16 : nShortest + 1) // good, we a +1 to the shortest one but only 16 rounds max allowed
                                                             :
                                                             0; // too bad, we are the fist one in that chain
        LogPrint("obfuscation", "GetInputObfuscationRounds UPDATED   %s %3d %3d\n", hash.ToString(), nout, mDenomWtxes[hash].vout[nout].nRounds);
        return mDenomWtxes[hash].vout[nout].nRounds;
    }

    return rounds - 1;
}

// respect current settings
int CWallet::GetInputObfuscationRounds(CTxIn in) const
{
    LOCK(cs_wallet);
    int realObfuscationRounds = GetRealInputObfuscationRounds(in, 0);
    return realObfuscationRounds > nVidulumSendRounds ? nVidulumSendRounds : realObfuscationRounds;
}

bool CWallet::IsDenominated(const CTxIn& txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end()) {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size()) return IsDenominatedAmount(prev.vout[txin.prevout.n].nValue);
        }
    }
    return false;
}

bool CWallet::IsDenominated(const CTransaction& tx) const
{
    /*
        Return false if ANY inputs are non-denom
    */
    bool ret = true;
    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
        if (!IsDenominated(txin)) {
            ret = false;
        }
    }
    return ret;
}


bool CWallet::IsDenominatedAmount(CAmount nInputAmount) const
{
    BOOST_FOREACH (CAmount d, obfuScationDenominations)
        if (nInputAmount == d)
            return true;
    return false;
}

bool CWallet::IsChange(const CTxOut& txout) const
{
    // TODO: fix handling of 'change' outputs. The assumption is that any
    // payment to a script that is ours, but is not in the address book
    // is change. That assumption is likely to break when we implement multisignature
    // wallets that return change back into a multi-signature-protected address;
    // a better way of identifying which outputs are 'the send' and which are
    // 'the change' will need to be implemented (maybe extend CWalletTx to remember
    // which output, if any, was change).
    if (::IsMine(*this, txout.scriptPubKey))
    {
        CTxDestination address;
        if (!ExtractDestination(txout.scriptPubKey, address))
            return true;

        LOCK(cs_wallet);
        if (!mapAddressBook.count(address))
            return true;
    }
    return false;
}


// Return sum of unlocked coins
CAmount CWalletTx::GetUnlockedCredit() const
{
    if (pwallet == 0)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < vout.size(); i++) {
        const CTxOut& txout = vout[i];

        if (pwallet->IsSpent(hashTx, i) || pwallet->IsLockedCoin(hashTx, i)) continue;
        if (fMasterNode && vout[i].nValue == Params().GetMasternodeCollateral() * COIN) continue; // do not count MN-like outputs

        nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE);
        if (!MoneyRange(nCredit))
            throw std::runtime_error("CWalletTx::GetUnlockedCredit() : value out of range");
    }

    return nCredit;
}

 // Return sum of locked coins
CAmount CWalletTx::GetLockedCredit() const
{
    if (pwallet == 0)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < vout.size(); i++) {
        const CTxOut& txout = vout[i];

        // Skip spent coins
        if (pwallet->IsSpent(hashTx, i)) continue;

        // Add locked coins
        if (pwallet->IsLockedCoin(hashTx, i)) {
            nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE);
        }

        // Add masternode collaterals which are handled like locked coins
        // else if (fMasterNode && vout[i].nValue == Params().GetMasternodeCollateral() * COIN) {
        //     nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE);
        // }

        if (!MoneyRange(nCredit))
            throw std::runtime_error("CWalletTx::GetLockedCredit() : value out of range");
    }

    return nCredit;
}

CAmount CWalletTx::GetDenominatedCredit(bool unconfirmed, bool fUseCache) const
{
    if (pwallet == 0)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    int nDepth = GetDepthInMainChain(false);
    if (nDepth < 0) return 0;

        // CheckFinalTx() uses chainActive.Height()+1 to evaluate
    // nLockTime because when IsFinalTx() is called within
    // CBlock::AcceptBlock(), the height of the block *being*
    // evaluated is what is used. Thus if we want to know if a
    // transaction can be part of the *next* block, we need to call
    // IsFinalTx() with one more than chainActive.Height().
    const int nBlockHeight = chainActive.Height() + 1;

    // Timestamps on the other hand don't get any special treatment,
    // because we can't know what timestamp the next block will have,
    // and there aren't timestamp applications where it matters.
    // However this changes once median past time-locks are enforced:
    const int64_t nBlockTime = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                             ? chainActive.Tip()->GetMedianTimePast()
                             : GetAdjustedTime();

    bool isUnconfirmed = !IsFinalTx(*this, nBlockHeight, nBlockTime) || (!IsTrusted() && nDepth == 0);
    if (unconfirmed != isUnconfirmed) return 0;

    if (fUseCache) {
        if (unconfirmed && fDenomUnconfCreditCached)
            return nDenomUnconfCreditCached;
        else if (!unconfirmed && fDenomConfCreditCached)
            return nDenomConfCreditCached;
    }

    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < vout.size(); i++) {
        const CTxOut& txout = vout[i];

        if (pwallet->IsSpent(hashTx, i) || !pwallet->IsDenominatedAmount(vout[i].nValue)) continue;

        nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE);
        if (!MoneyRange(nCredit))
            throw std::runtime_error("CWalletTx::GetDenominatedCredit() : value out of range");
    }

    if (unconfirmed) {
        nDenomUnconfCreditCached = nCredit;
        fDenomUnconfCreditCached = true;
    } else {
        nDenomConfCreditCached = nCredit;
        fDenomConfCreditCached = true;
    }
    return nCredit;
}

CAmount CWalletTx::GetAnonymizedCredit(bool fUseCache) const
{
    if (pwallet == 0)
        return 0;

    // Must wait until coinbase is safely deep enough in the chain before valuing it
    if (IsCoinBase() && GetBlocksToMaturity() > 0)
        return 0;

    if (fUseCache && fAnonymizedCreditCached)
        return nAnonymizedCreditCached;

    CAmount nCredit = 0;
    uint256 hashTx = GetHash();
    for (unsigned int i = 0; i < vout.size(); i++) {
        const CTxOut& txout = vout[i];
        const CTxIn vin = CTxIn(hashTx, i);

        if (pwallet->IsSpent(hashTx, i) || !pwallet->IsDenominated(vin)) continue;

        const int rounds = pwallet->GetInputObfuscationRounds(vin);
        if (rounds >= nVidulumSendRounds) {
            nCredit += pwallet->GetCredit(txout, ISMINE_SPENDABLE);
            if (!MoneyRange(nCredit))
                throw std::runtime_error("CWalletTx::GetAnonymizedCredit() : value out of range");
        }
    }

    nAnonymizedCreditCached = nCredit;
    fAnonymizedCreditCached = true;
    return nCredit;
}

CAmount CWallet::GetChange(const CTxOut& txout) const
{
    if (!MoneyRange(txout.nValue))
        throw std::runtime_error("CWallet::GetChange(): value out of range");
    return (IsChange(txout) ? txout.nValue : 0);
}

bool CWallet::IsMine(const CTransaction& tx) const
{
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
        if (IsMine(txout))
            return true;
    return false;
}

bool CWallet::IsFromMe(const CTransaction& tx) const
{
    if (GetDebit(tx, ISMINE_ALL) > 0) {
        return true;
    }
    for (const JSDescription& jsdesc : tx.vjoinsplit) {
        for (const uint256& nullifier : jsdesc.nullifiers) {
            if (IsSproutNullifierFromMe(nullifier)) {
                return true;
            }
        }
    }
    for (const SpendDescription &spend : tx.vShieldedSpend) {
        if (IsSaplingNullifierFromMe(spend.nullifier)) {
            return true;
        }
    }
    return false;
}

CAmount CWallet::GetDebit(const CTransaction& tx, const isminefilter& filter) const
{
    CAmount nDebit = 0;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        nDebit += GetDebit(txin, filter);
        if (!MoneyRange(nDebit))
            throw std::runtime_error("CWallet::GetDebit(): value out of range");
    }
    return nDebit;
}

CAmount CWallet::GetCredit(const CTransaction& tx, const isminefilter& filter) const
{
    CAmount nCredit = 0;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        nCredit += GetCredit(txout, filter);
        if (!MoneyRange(nCredit))
            throw std::runtime_error("CWallet::GetCredit(): value out of range");
    }
    return nCredit;
}

CAmount CWallet::GetChange(const CTransaction& tx) const
{
    CAmount nChange = 0;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        nChange += GetChange(txout);
        if (!MoneyRange(nChange))
            throw std::runtime_error("CWallet::GetChange(): value out of range");
    }
    return nChange;
}

bool CWallet::IsHDFullyEnabled() const
{
    // Only Sapling addresses are HD for now
    return false;
}

void CWallet::GenerateNewSeed()
{
    LOCK(cs_wallet);

    auto seed = HDSeed::Random(HD_WALLET_SEED_LENGTH);

    int64_t nCreationTime = GetTime();

    // If the wallet is encrypted and locked, this will fail.
    if (!SetHDSeed(seed))
        throw std::runtime_error(std::string(__func__) + ": SetHDSeed failed");

    // store the key creation time together with
    // the child index counter in the database
    // as a hdchain object
    CHDChain newHdChain;
    newHdChain.nVersion = CHDChain::VERSION_HD_BASE;
    newHdChain.seedFp = seed.Fingerprint();
    newHdChain.nCreateTime = nCreationTime;
    SetHDChain(newHdChain, false);
}

bool CWallet::SetHDSeed(const HDSeed& seed)
{
    if (!CCryptoKeyStore::SetHDSeed(seed)) {
        return false;
    }

    if (!fFileBacked) {
        return true;
    }

    {
        LOCK(cs_wallet);
        if (!IsCrypted()) {
            return CWalletDB(strWalletFile).WriteHDSeed(seed);
        }
    }
    return true;
}

bool CWallet::SetCryptedHDSeed(const uint256& seedFp, const std::vector<unsigned char> &vchCryptedSecret)
{
    if (!CCryptoKeyStore::SetCryptedHDSeed(seedFp, vchCryptedSecret)) {
        return false;
    }

    if (!fFileBacked) {
        return true;
    }

    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption)
            return pwalletdbEncryption->WriteCryptedHDSeed(seedFp, vchCryptedSecret);
        else
            return CWalletDB(strWalletFile).WriteCryptedHDSeed(seedFp, vchCryptedSecret);
    }
    return false;
}

void CWallet::SetHDChain(const CHDChain& chain, bool memonly)
{
    LOCK(cs_wallet);
    if (!memonly && fFileBacked && !CWalletDB(strWalletFile).WriteHDChain(chain))
        throw std::runtime_error(std::string(__func__) + ": writing chain failed");

    hdChain = chain;
}

bool CWallet::LoadHDSeed(const HDSeed& seed)
{
    return CBasicKeyStore::SetHDSeed(seed);
}

bool CWallet::LoadCryptedHDSeed(const uint256& seedFp, const std::vector<unsigned char>& seed)
{
    return CCryptoKeyStore::SetCryptedHDSeed(seedFp, seed);
}

void CWalletTx::SetSproutNoteData(mapSproutNoteData_t &noteData)
{
    mapSproutNoteData.clear();
    for (const std::pair<JSOutPoint, SproutNoteData> nd : noteData) {
        if (nd.first.js < vjoinsplit.size() &&
                nd.first.n < vjoinsplit[nd.first.js].ciphertexts.size()) {
            // Store the address and nullifier for the Note
            mapSproutNoteData[nd.first] = nd.second;
        } else {
            // If FindMySproutNotes() was used to obtain noteData,
            // this should never happen
            throw std::logic_error("CWalletTx::SetSproutNoteData(): Invalid note");
        }
    }
}

void CWalletTx::SetSaplingNoteData(mapSaplingNoteData_t &noteData)
{
    mapSaplingNoteData.clear();
    for (const std::pair<SaplingOutPoint, SaplingNoteData> nd : noteData) {
        if (nd.first.n < vShieldedOutput.size()) {
            mapSaplingNoteData[nd.first] = nd.second;
        } else {
            throw std::logic_error("CWalletTx::SetSaplingNoteData(): Invalid note");
        }
    }
}

int64_t CWalletTx::GetTxTime() const
{
    int64_t n = nTimeSmart;
    return n ? n : nTimeReceived;
}

int CWalletTx::GetRequestCount() const
{
    // Returns -1 if it wasn't being tracked
    int nRequests = -1;
    {
        LOCK(pwallet->cs_wallet);
        if (IsCoinBase())
        {
            // Generated block
            if (!hashBlock.IsNull())
            {
                map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                if (mi != pwallet->mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        }
        else
        {
            // Did anyone request this transaction?
            map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(GetHash());
            if (mi != pwallet->mapRequestCount.end())
            {
                nRequests = (*mi).second;

                // How about the block it's in?
                if (nRequests == 0 && !hashBlock.IsNull())
                {
                    map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                    if (mi != pwallet->mapRequestCount.end())
                        nRequests = (*mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}

// GetAmounts will determine the transparent debits and credits for a given wallet tx.
void CWalletTx::GetAmounts(list<COutputEntry>& listReceived,
                           list<COutputEntry>& listSent, CAmount& nFee, string& strSentAccount, const isminefilter& filter) const
{
    nFee = 0;
    listReceived.clear();
    listSent.clear();
    strSentAccount = strFromAccount;

    // Is this tx sent/signed by me?
    CAmount nDebit = GetDebit(filter);
    bool isFromMyTaddr = nDebit > 0; // debit>0 means we signed/sent this transaction

    // Compute fee if we sent this transaction.
    if (isFromMyTaddr) {
        CAmount nValueOut = GetValueOut();  // transparent outputs plus all Sprout vpub_old and negative Sapling valueBalance
        CAmount nValueIn = GetShieldedValueIn();
        nFee = nDebit - nValueOut + nValueIn;
    }

    // Create output entry for vpub_old/new, if we sent utxos from this transaction
    if (isFromMyTaddr) {
        CAmount myVpubOld = 0;
        CAmount myVpubNew = 0;
        for (const JSDescription& js : vjoinsplit) {
            bool fMyJSDesc = false;

            // Check input side
            for (const uint256& nullifier : js.nullifiers) {
                if (pwallet->IsSproutNullifierFromMe(nullifier)) {
                    fMyJSDesc = true;
                    break;
                }
            }

            // Check output side
            if (!fMyJSDesc) {
                for (const std::pair<JSOutPoint, SproutNoteData> nd : this->mapSproutNoteData) {
                    if (nd.first.js < vjoinsplit.size() && nd.first.n < vjoinsplit[nd.first.js].ciphertexts.size()) {
                        fMyJSDesc = true;
                        break;
                    }
                }
            }

            if (fMyJSDesc) {
                myVpubOld += js.vpub_old;
                myVpubNew += js.vpub_new;
            }

            if (!MoneyRange(js.vpub_old) || !MoneyRange(js.vpub_new) || !MoneyRange(myVpubOld) || !MoneyRange(myVpubNew)) {
                 throw std::runtime_error("CWalletTx::GetAmounts: value out of range");
            }
        }

        // Create an output for the value taken from or added to the transparent value pool by JoinSplits
        if (myVpubOld > myVpubNew) {
            COutputEntry output = {CNoDestination(), myVpubOld - myVpubNew, (int)vout.size()};
            listSent.push_back(output);
        } else if (myVpubNew > myVpubOld) {
            COutputEntry output = {CNoDestination(