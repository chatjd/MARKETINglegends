// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coins.h"

#include "memusage.h"
#include "random.h"
#include "version.h"
#include "policy/fees.h"

#include <assert.h>

/**
 * calculate number of bytes for the bitmask, and its number of non-zero bytes
 * each bit in the bitmask represents the availability of one output, but the
 * availabilities of the first two outputs are encoded separately
 */
void CCoins::CalcMaskSize(unsigned int &nBytes, unsigned int &nNonzeroBytes) const {
    unsigned int nLastUsedByte = 0;
    for (unsigned int b = 0; 2+b*8 < vout.size(); b++) {
        bool fZero = true;
        for (unsigned int i = 0; i < 8 && 2+b*8+i < vout.size(); i++) {
            if (!vout[2+b*8+i].IsNull()) {
                fZero = false;
                continue;
            }
        }
        if (!fZero) {
            nLastUsedByte = b + 1;
            nNonzeroBytes++;
        }
    }
    nBytes += nLastUsedByte;
}

bool CCoins::Spend(uint32_t nPos) 
{
    if (nPos >= vout.size() || vout[nPos].IsNull())
        return false;
    vout[nPos].SetNull();
    Cleanup();
    return true;
}
bool CCoinsView::GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const { return false; }
bool CCoinsView::GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const { return false; }
bool CCoinsView::GetNullifier(const uint256 &nullifier, ShieldedType type) const { return false; }
bool CCoinsView::GetCoins(const uint256 &txid, CCoins &coins) const { return false; }
bool CCoinsView::HaveCoins(const uint256 &txid) const { return false; }
uint256 CCoinsView::GetBestBlock() const { return uint256(); }
uint256 CCoinsView::GetBestAnchor(ShieldedType type) const { return uint256(); };
bool CCoinsView::BatchWrite(CCoinsMap &mapCoins,
                            const uint256 &hashBlock,
                            const uint256 &hashSproutAnchor,
                            const uint256 &hashSaplingAnchor,
                            CAnchorsSproutMap &mapSproutAnchors,
                            CAnchorsSaplingMap &mapSaplingAnchors,
                            CNullifiersMap &mapSproutNullifiers,
                            CNullifiersMap &mapSaplingNullifiers) { return false; }
bool CCoinsView::GetStats(CCoinsStats &stats) const { return false; }


CCoinsViewBacked::CCoinsViewBacked(CCoinsView *viewIn) : base(viewIn) { }

bool CCoinsViewBacked::GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const { return base->GetSproutAnchorAt(rt, tree); }
bool CCoinsViewBacked::GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const { return base->GetSaplingAnchorAt(rt, tree); }
bool CCoinsViewBacked::GetNullifier(const uint256 &nullifier, ShieldedType type) const { return base->GetNullifier(nullifier, type); }
bool CCoinsViewBacked::GetCoins(const uint256 &txid, CCoins &coins) const { return base->GetCoins(txid, coins); }
bool CCoinsViewBacked::HaveCoins(const uint256 &txid) const { return base->HaveCoins(txid); }
uint256 CCoinsViewBacked::GetBestBlock() const { return base->GetBestBlock(); }
uint256 CCoinsViewBacked::GetBestAnchor(ShieldedType type) const { return base->GetBestAnchor(type); }
void CCoinsViewBacked::SetBackend(CCoinsView &viewIn) { base = &viewIn; }
bool CCoinsViewBacked::BatchWrite(CCoinsMap &mapCoins,
                                  const uint256 &hashBlock,
                                  const uint256 &hashSproutAnchor,
                                  const uint256 &hashSaplingAnchor,
                                  CAnchorsSproutMap &mapSproutAnchors,
                                  CAnchorsSaplingMap &mapSaplingAnchors,
                                  CNullifiersMap &mapSproutNullifiers,
                                  CNullifiersMap &mapSaplingNullifiers) { return base->BatchWrite(mapCoins, hashBlock, hashSproutAnchor, hashSaplingAnchor, mapSproutAnchors, mapSaplingAnchors, mapSproutNullifiers, mapSaplingNullifiers); }
bool CCoinsViewBacked::GetStats(CCoinsStats &stats) const { return base->GetStats(stats); }

CCoinsKeyHasher::CCoinsKeyHasher() : salt(GetRandHash()) {}

CCoinsViewCache::CCoinsViewCache(CCoinsView *baseIn) : CCoinsViewBacked(baseIn), hasModifier(false), cachedCoinsUsage(0) { }

CCoinsViewCache::~CCoinsViewCache()
{
    assert(!hasModifier);
}

size_t CCoinsViewCache::DynamicMemoryUsage() const {
    return memusage::DynamicUsage(cacheCoins) +
           memusage::DynamicUsage(cacheSproutAnchors) +
           memusage::DynamicUsage(cacheSaplingAnchors) +
           memusage::DynamicUsage(cacheSproutNullifiers) +
           memusage::DynamicUsage(cacheSaplingNullifiers) +
           cachedCoinsUsage;
}

CCoinsMap::const_iterator CCoinsViewCache::FetchCoins(const uint256 &txid) const {
    CCoinsMap::iterator it = cacheCoins.find(txid);
    if (it != cacheCoins.end())
        return it;
    CCoins tmp;
    if (!base->GetCoins(txid, tmp))
        return cacheCoins.end();
    CCoinsMap::iterator ret = cacheCoins.insert(std::make_pair(txid, CCoinsCacheEntry())).first;
    tmp.swap(ret->second.coins);
    if (ret->second.coins.IsPruned()) {
        // The parent only has an empty entry for this txid; we can consider our
        // version as fresh.
        ret->second.flags = CCoinsCacheEntry::FRESH;
    }
    cachedCoinsUsage += ret->second.coins.DynamicMemoryUsage();
    return ret;
}


bool CCoinsViewCache::GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const {
    CAnchorsSproutMap::const_iterator it = cacheSproutAnchors.find(rt);
    if (it != cacheSproutAnchors.end()) {
        if (it->second.entered) {
            tree = it->second.tree;
            return true;
        } else {
            return false;
        }
    }

    if (!base->GetSproutAnchorAt(rt, tree)) {
        return false;
    }

    CAnchorsSproutMap::iterator ret = cacheSproutAnchors.insert(std::make_pair(rt, CAnchorsSproutCacheEntry())).first;
    ret->second.entered = true;
    ret->second.tree = tree;
    cachedCoinsUsage += ret->second.tree.DynamicMemoryUsage();

    return true;
}

bool CCoinsViewCache::GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const {
    CAnchorsSaplingMap::const_iterator it = cacheSaplingAnchors.find(rt);
    if (it != cacheSaplingAnchors.end()) {
        if (it->second.entered) {
            tree = it->second.tree;
            return true;
        } else {
            return false;
        }
    }

    if (!base->GetSaplingAnchorAt(rt, tree)) {
        return false;
    }

    CAnchorsSaplingMap::iterator ret = cacheSaplingAnchors.insert(std::make_pair(rt, CAnchorsSaplingCacheEntry())).first;
    ret->second.entered = true;
    ret->second.tree = tree;
    cachedCoinsUsage += ret->second.tree.DynamicMemoryUsage();

    return true;
}

bool CCoinsViewCache::GetNullifier(const uint256 &nullifier, ShieldedType type) const {
    CNullifiersMap* cacheToUse;
    switch (type) {
        case SPROUT:
            cacheToUse = &cacheSproutNullifiers;
            break;
        case SAPLING:
            cacheToUse = &cacheSaplingNullifiers;
            break;
        default:
            throw std::runtime_error("Unknown shielded type");
    }
    CNullifiersMap::iterator it = cacheToUse->find(nullifier);
    if (it != cacheToUse->end())
        return it->second.entered;

    CNullifiersCacheEntry entry;
    bool tmp = base->GetNullifier(nullifier, type);
    entry.entered = tmp;

    cacheToUse->insert(std::make_pair(nullifier, entry));

    return tmp;
}

template<typename Tree, typename Cache, typename CacheIterator, typename CacheEntry>
void CCoinsViewCache::AbstractPushAnchor(
    const Tree &tree,
    ShieldedType type,
    Cache &cacheAnchors,
    uint256 &hash
)
{
    uint256 newrt = tree.root();

    auto currentRoot = GetBestAnchor(type);

    // We don't want to overwrite an anchor we already have.
    // This occurs when a block doesn't modify mapAnchors at all,
    // because there are no joinsplits. We could get around this a
    // different way (make all blocks modify mapAnchors somehow)
    // but this is simpler to reason about.
    if (currentRoot != newrt) {
        auto insertRet = cacheAnchors.insert(std::make_pair(newrt, CacheEntry()));
        CacheIterator ret = insertRet.first;

        ret->second.entered = true;
        ret->second.tree = tree;
        ret->second.flags = CacheEntry::DIRTY;

        if (insertRet.second) {
            // An insert took place
            cachedCoinsUsage += ret->second.tree.DynamicMemoryUsage();
        }

        hash = newrt;
    }
}

template<> void CCoinsViewCache::PushAnchor(const SproutMerkleTree &tree)
{
    AbstractPushAnchor<SproutMerkleTree, CAnchorsSproutMap, CAnchorsSproutMap::iterator, CAnchorsSproutCacheEntry>(
        tree,
        SPROUT,
        cacheSproutAnchors,
        hashSproutAnchor
    );
}

template<> void CCoinsViewCache::PushAnchor(const SaplingMerkleTree &tree)
{
    AbstractPushAnchor<SaplingMerkleTree, CAnchorsSaplingMap, CAnchorsSaplingMap::iterator, CAnchorsSaplingCacheEntry>(
        tree,
        SAPLING,
        cacheSaplingAnchors,
        hashSaplingAnchor
    );
}

template<>
void CCoinsViewCache::BringBestAnchorIntoCache(
    const uint256 &currentRoot,
    SproutMerkleTree &tree
)
{
    assert(GetSproutAnchorAt(currentRoot, tree));
}

template<>
void CCoinsViewCache::BringBestAnchorIntoCache(
    const uint256 &currentRoot,
    SaplingMerkleTree &tree
)
{
    assert(GetSaplingAnchorAt(currentRoot, tree));
}

template<typename Tree, typename Cache, typename CacheEntry>
void CCoinsViewCache::AbstractPopAnchor(
    const uint256 &newrt,
    ShieldedType type,
    Cache &cacheAnchors,
    uint256 &hash
)
{
    auto currentRoot = GetBestAnchor(type);

    // Blocks might not change the commitment tree, in which
    // case restoring the "old" anchor during a reorg must
    // have no effect.
    if (currentRoot != newrt) {
        // Bring the current best anchor into our local cache
        // so that its tree exists in memory.
        {
            Tree tree;
            BringBestAnchorIntoCache(currentRoot, tree);
        }

        // Mark the anchor as unentered, removing it from view
        cacheAnchors[currentRoot].entered = false;

        // Mark the cache entry as dirty so it's propagated
        cacheAnchors[currentRoot].flags = CacheEntry::DIRTY;

        // Mark the new root as the best anchor
        hash = newrt;
    }
}

void CCoinsViewCache::PopAnchor(const uint256 &newrt, ShieldedType type) {
    switch (type) {
        case SPROUT:
            AbstractPopAnchor<SproutMerkleTree, CAnchorsSproutMap, CAnchorsSproutCacheEntry>(
                newrt,
                SPROUT,
                cacheSproutAnchors,
                hashSproutAnchor
            );
            break;
        case SAPLING:
            AbstractPopAnchor<SaplingMerkleTree, CAnchorsSaplingMap, CAnchorsSaplingCacheEntry>(
                newrt,
                SAPLING,
                cacheSaplingAnchors,
                hashSaplingAnchor
            );
            break;
        default:
            throw std::runtime_error("Unknown shielded type");
    }
}

void CCoinsViewCache::SetNullifiers(const CTransaction& tx, bool spent) {
    for (const JSDescription &joinsplit : tx.vjoinsplit) {
        for (const uint256 &nullifier : joinsplit.nullifiers) {
            std::pair<CNullifiersMap::iterator, bool> ret = cacheSproutNullifiers.insert(std::make_pair(nullifier, CNullifiersCacheEntry()));
            ret.first->second.entered = spent;
            ret.first->second.flags |= CNullifiersCacheEntry::DIRTY;
        }
    }
    for (const SpendDescription &spendDescription : tx.vShieldedSpend) {
        std::pair<CNullifiersMap::iterator, bool> ret = cacheSaplingNullifiers.insert(std::make_pair(spendDescription.nullifier, CNullifiersCacheEntry()));
        ret.first->second.entered = spent;
        ret.first->second.flags |= CNullifiersCacheEntry::DIRTY;
    }
}

bool CCoinsViewCache::GetCoins(const uint256 &txid, CCoins &coins) const {
    CCoinsMap::const_iterator it = FetchCoins(txid);
    if (it != cacheCoins.end()) {
        coins = it->second.coins;
        return true;
    }
    return false;
}

CCoinsModifier CCoinsViewCache::ModifyCoins(const uint256 &txid) {
    assert(!hasModifier);
    std::pair<CCoinsMap::iterator, bool> ret = cacheCoins.insert(std::make_pair(txid, CCoinsCacheEntry()));
    size_t cachedCoinUsage = 0;
    if (ret.second) {
        if (!base->GetCoins(txid, ret.first->second.coins)) {
            // The parent view does not have this entry; mark it as fresh.
            ret.first->second.coins.Clear();
            ret.first->second.flags = CCoinsCacheEntry::FRESH;
        } else if (ret.first->second.coins.IsPruned()) {
            // The parent view only has a pruned entry for this; mark it as fresh.
            ret.first->second.flags = CCoinsCacheEntry::FRESH;
        }
    } else {
        cachedCoinUsage = ret.first->second.coins.DynamicMemoryUsage();
    }
    // Assume that whenever ModifyCoins is called, the entry will be modified.
    ret.first->second.flags |= CCoinsCacheEntry::DIRTY;
    return CCoinsModifier(*this, ret.first, cachedCoinUsage);
}

const CCoins* CCoinsViewCache::AccessCoins(const uint256 &txid) const {
    CCoinsMap::const_iterator it = FetchCoins(txid);
    if (it == cacheCoins.end()) {
        return NULL;
    } else {
        return &it->second.coins;
    }
}

bool CCoinsViewCache::HaveCoins(const uint256 &txid) const {
    CCoinsMap::const_iterator it = FetchCoins(txid);
    // We're using vtx.empty() instead of IsPruned here for performance reasons,
    // as we only care about the case where a transaction was replaced entirely
    // in a reorganization (which wipes vout entirely, as opposed to spending
    // which just cleans individual outputs).
    return (it != cacheCoins.end() && !it->second.coins.vout.empty());
}

uint256 CCoinsViewCache::GetBestBlock() const {
    if (hashBlock.IsNull())
        hashBlock = base->GetBestBlock();
    return hashBlock;
}


uint256 CCoinsViewCache::GetBestAnchor(ShieldedType type) const {
    switch (type) {
        case SPROUT:
            if (hashSproutAnchor.IsNull())
                hashSproutAnchor = base->GetBestAnchor(type);
            return hashSproutAnchor;
            break;
        case SAPLING:
            if (hashSaplingAnchor.IsNull())
                hashSaplingAnchor = base->GetBestAnchor(type);
            return hashSaplingAnchor;
            break;
        default:
            throw std::runtime_error("Unknown shielded type");
    }
}

void CCoinsViewCache::SetBestBlock(const uint256 &hashBlockIn) {
    hashBlock = hashBlockIn;
}

void BatchWriteNullifiers(CNullifiersMap &mapNullifiers, CNullifiersMap &cacheNullifiers)
{
    for (CNullifiersMap::iterator ch