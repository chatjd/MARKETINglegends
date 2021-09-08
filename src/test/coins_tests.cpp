// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coins.h"
#include "random.h"
#include "script/standard.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "test/test_bitcoin.h"
#include "consensus/validation.h"
#include "main.h"
#include "undo.h"
#include "primitives/transaction.h"
#include "pubkey.h"

#include <vector>
#include <map>

#include <boost/test/unit_test.hpp>
#include "vidulum/IncrementalMerkleTree.hpp"

namespace
{
class CCoinsViewTest : public CCoinsView
{
    uint256 hashBestBlock_;
    uint256 hashBestSproutAnchor_;
    uint256 hashBestSaplingAnchor_;
    std::map<uint256, CCoins> map_;
    std::map<uint256, SproutMerkleTree> mapSproutAnchors_;
    std::map<uint256, SaplingMerkleTree> mapSaplingAnchors_;
    std::map<uint256, bool> mapSproutNullifiers_;
    std::map<uint256, bool> mapSaplingNullifiers_;

public:
    CCoinsViewTest() {
        hashBestSproutAnchor_ = SproutMerkleTree::empty_root();
        hashBestSaplingAnchor_ = SaplingMerkleTree::empty_root();
    }

    bool GetSproutAnchorAt(const uint256& rt, SproutMerkleTree &tree) const {
        if (rt == SproutMerkleTree::empty_root()) {
            SproutMerkleTree new_tree;
            tree = new_tree;
            return true;
        }

        std::map<uint256, SproutMerkleTree>::const_iterator it = mapSproutAnchors_.find(rt);
        if (it == mapSproutAnchors_.end()) {
            return false;
        } else {
            tree = it->second;
            return true;
        }
    }

    bool GetSaplingAnchorAt(const uint256& rt, SaplingMerkleTree &tree) const {
        if (rt == SaplingMerkleTree::empty_root()) {
            SaplingMerkleTree new_tree;
            tree = new_tree;
            return true;
        }

        std::map<uint256, SaplingMerkleTree>::const_iterator it = mapSaplingAnchors_.find(rt);
        if (it == mapSaplingAnchors_.end()) {
            return false;
        } else {
            tree = it->second;
            return true;
        }
    }

    bool GetNullifier(const uint256 &nf, ShieldedType type) const
    {
        const std::map<uint256, bool>* mapToUse;
        switch (type) {
            case SPROUT:
                mapToUse = &mapSproutNullifiers_;
                break;
            case SAPLING:
                mapToUse = &mapSaplingNullifiers_;
                break;
            default:
                throw std::runtime_error("Unknown shielded type");
        }
        std::map<uint256, bool>::const_iterator it = mapToUse->find(nf);
        if (it == mapToUse->end()) {
            return false;
        } else {
            // The map shouldn't contain any false entries.
            assert(it->second);
            return true;
        }
    }

    uint256 GetBestAnchor(ShieldedType type) const {
        switch (type) {
            case SPROUT:
                return hashBestSproutAnchor_;
                break;
            case SAPLING:
                return hashBestSaplingAnchor_;
                break;
            default:
                throw std::runtime_error("Unknown shielded type");
        }
    }

    bool GetCoins(const uint256& txid, CCoins& coins) const
    {
        std::map<uint256, CCoins>::const_iterator it = map_.find(txid);
        if (it == map_.end()) {
            return false;
        }
        coins = it->second;
        if (coins.IsPruned() && insecure_rand() % 2 == 0) {
            // Randomly return false in case of an empty entry.
            return false;
        }
        return true;
    }

    bool HaveCoins(const uint256& txid) const
    {
        CCoins coins;
        return GetCoins(txid, coins);
    }

    uint256 GetBestBlock() const { return hashBestBlock_; }

    void BatchWriteNullifiers(CNullifiersMap& mapNullifiers, std::map<uint256, bool>& cacheNullifiers)
    {
        for (CNullifiersMap::iterator it = mapNullifiers.begin(); it != mapNullifiers.end(); ) {
            if (it->second.entered) {
                cacheNullifiers[it->first] = true;
            } else {
                cacheNullifiers.erase(it->first);
            }
            mapNullifiers.erase(it++);
        }
        mapNullifiers.clear();
    }

    template<typename Tree, typename Map>
    void BatchWriteAnchors(Map& mapAnchors, std::map<uint256, Tree>& cacheAnchors)
    {
        for (auto it = mapAnchors.begin(); it != mapAnchors.end(); ) {
            if (it->second.entered) {
                auto ret = cacheAnchors.insert(std::make_pair(it->first, Tree())).first;
                ret->second = it->second.tree;
            } else {
                cacheAnchors.erase(it->first);
            }
            mapAnchors.erase(it++);
        }
    }

    bool BatchWrite(CCoinsMap& mapCoins,
                    const uint256& hashBlock,
                    const uint256& hashSproutAnchor,
                    const uint256& hashSaplingAnchor,
                    CAnchorsSproutMap& mapSproutAnchors,
                    CAnchorsSaplingMap& mapSaplingAnchors,
                    CNullifiersMap& mapSproutNullifiers,
                    CNullifiersMap& mapSaplingNullifiers)
    {
        for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end(); ) {
            map_[it->first] = it->second.coins;
            if (it->second.coins.IsPruned() && insecure_rand() % 3 == 0) {
                // Randomly delete empty entries on write.
                map_.erase(it->first);
            }
            mapCoins.erase(it++);
        }

        BatchWriteAnchors<SproutMerkleTree, CAnchorsSproutMap>(mapSproutAnchors, mapSproutAnchors_);
        BatchWriteAnchors<SaplingMerkleTree, CAnchorsSaplingMap>(mapSaplingAnchors, mapSaplingAnchors_);

        BatchWriteNullifiers(mapSproutNullifiers, mapSproutNullifiers_);
        BatchWriteNullifiers(mapSaplingNullifiers, mapSaplingNullifiers_);

        mapCoins.clear();
        mapSproutAnchors.clear();
        mapSaplingAnchors.clear();
        hashBestBlock_ = hashBlock;
        hashBestSproutAnchor_ = hashSproutAnchor;
        hashBestSaplingAnchor_ = hashSaplingAnchor;
        return true;
    }

    bool GetStats(CCoinsStats& stats) const { return false; }
};

class CCoinsViewCacheTest : public CCoinsViewCache
{
public:
    CCoinsViewCacheTest(CCoinsView* base) : CCoinsViewCache(base) {}

    void SelfTest() const
    {
        // Manually recompute the dynamic usage of the whole data, and compare it.
        size_t ret = memusage::DynamicUsage(cacheCoins) +
                     memusage::DynamicUsage(cacheSproutAnchors) +
                     memusage::DynamicUsage(cacheSaplingAnchors) +
                     memusage::DynamicUsage(cacheSproutNullifiers) +
                     memusage::DynamicUsage(cacheSaplingNullifiers);
        for (CCoinsMap::iterator it = cacheCoins.begin(); it != cacheCoins.end(); it++) {
            ret += it->second.coins.DynamicMemoryUsage();
        }
        BOOST_CHECK_EQUAL(DynamicMemoryUsage(), ret);
    }

};

class TxWithNullifiers
{
public:
    CTransaction tx;
    uint256 sproutNullifier;
    uint256 saplingNullifier;

    TxWithNullifiers()
    {
        CMutableTransaction mutableTx;

        sproutNullifier = GetRandHash();
        JSDescription jsd;
        jsd.nullifiers[0] = sproutNullifier;
        mutableTx.vjoinsplit.emplace_back(jsd);
        
        saplingNullifier = GetRandHash();
        SpendDescription sd;
        sd.nullifier = saplingNullifier;
        mutableTx.vShieldedSpend.push_back(sd);

        tx = CTransaction(mutableTx);
    }
};

}

uint256 appendRandomSproutCommitment(SproutMerkleTree &tree)
{
    libzcash::SproutSpendingKey k = libzcash::SproutSpendingKey::random();
    libzcash::SproutPaymentAddress addr = k.address();

    libzcash::SproutNote note(addr.a_pk, 0, uint256(), uint256());

    auto cm = note.cm();
    tree.append(cm);
    return cm;
}

template<typename Tree> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, Tree &tree);
template<> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, SproutMerkleTree &tree) { return cache.GetSproutAnchorAt(rt, tree); }
template<> bool GetAnchorAt(const CCoinsViewCacheTest &cache, const uint256 &rt, SaplingMerkleTree &tree) { return cache.GetSaplingAnchorAt(rt, tree); }

BOOST_FIXTURE_TEST_SUITE(coins_tests, BasicTestingSetup)

void checkNullifierCache(const CCoinsViewCacheTest &cache, const TxWithNullifiers &txWithNullifiers, bool shouldBeInCache) {
    // Make sure the nullifiers have not gotten mixed up
    BOOST_CHECK(!cache.GetNullifier(txWithNullifiers.s