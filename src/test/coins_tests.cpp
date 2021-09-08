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
    BOOST_CHECK(!cache.GetNullifier(txWithNullifiers.sproutNullifier, SAPLING));
    BOOST_CHECK(!cache.GetNullifier(txWithNullifiers.saplingNullifier, SPROUT));
    // Check if the nullifiers either are or are not in the cache
    bool containsSproutNullifier = cache.GetNullifier(txWithNullifiers.sproutNullifier, SPROUT);
    bool containsSaplingNullifier = cache.GetNullifier(txWithNullifiers.saplingNullifier, SAPLING);
    BOOST_CHECK(containsSproutNullifier == shouldBeInCache);
    BOOST_CHECK(containsSaplingNullifier == shouldBeInCache);
}

BOOST_AUTO_TEST_CASE(nullifier_regression_test)
{
    // Correct behavior:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        TxWithNullifiers txWithNullifiers;

        // Insert a nullifier into the base.
        cache1.SetNullifiers(txWithNullifiers.tx, true);
        checkNullifierCache(cache1, txWithNullifiers, true);
        cache1.Flush(); // Flush to base.

        // Remove the nullifier from cache
        cache1.SetNullifiers(txWithNullifiers.tx, false);

        // The nullifier now should be `false`.
        checkNullifierCache(cache1, txWithNullifiers, false);
    }

    // Also correct behavior:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        TxWithNullifiers txWithNullifiers;

        // Insert a nullifier into the base.
        cache1.SetNullifiers(txWithNullifiers.tx, true);
        checkNullifierCache(cache1, txWithNullifiers, true);
        cache1.Flush(); // Flush to base.

        // Remove the nullifier from cache
        cache1.SetNullifiers(txWithNullifiers.tx, false);
        cache1.Flush(); // Flush to base.

        // The nullifier now should be `false`.
        checkNullifierCache(cache1, txWithNullifiers, false);
    }

    // Works because we bring it from the parent cache:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert a nullifier into the base.
        TxWithNullifiers txWithNullifiers;
        cache1.SetNullifiers(txWithNullifiers.tx, true);
        checkNullifierCache(cache1, txWithNullifiers, true);
        cache1.Flush(); // Empties cache.

        // Create cache on top.
        {
            // Remove the nullifier.
            CCoinsViewCacheTest cache2(&cache1);
            checkNullifierCache(cache2, txWithNullifiers, true);
            cache1.SetNullifiers(txWithNullifiers.tx, false);
            cache2.Flush(); // Empties cache, flushes to cache1.
        }

        // The nullifier now should be `false`.
        checkNullifierCache(cache1, txWithNullifiers, false);
    }

    // Was broken:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert a nullifier into the base.
        TxWithNullifiers txWithNullifiers;
        cache1.SetNullifiers(txWithNullifiers.tx, true);
        cache1.Flush(); // Empties cache.

        // Create cache on top.
        {
            // Remove the nullifier.
            CCoinsViewCacheTest cache2(&cache1);
            cache2.SetNullifiers(txWithNullifiers.tx, false);
            cache2.Flush(); // Empties cache, flushes to cache1.
        }

        // The nullifier now should be `false`.
        checkNullifierCache(cache1, txWithNullifiers, false);
    }
}

template<typename Tree> void anchorPopRegressionTestImpl(ShieldedType type)
{
    // Correct behavior:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Create dummy anchor/commitment
        Tree tree;
        tree.append(GetRandHash());

        // Add the anchor
        cache1.PushAnchor(tree);
        cache1.Flush();

        // Remove the anchor
        cache1.PopAnchor(Tree::empty_root(), type);
        cache1.Flush();

        // Add the anchor back
        cache1.PushAnchor(tree);
        cache1.Flush();

        // The base contains the anchor, of course!
        {
            Tree checkTree;
            BOOST_CHECK(GetAnchorAt(cache1, tree.root(), checkTree));
            BOOST_CHECK(checkTree.root() == tree.root());
        }
    }

    // Previously incorrect behavior
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Create dummy anchor/commitment
        Tree tree;
        tree.append(GetRandHash());

        // Add the anchor and flush to disk
        cache1.PushAnchor(tree);
        cache1.Flush();

        // Remove the anchor, but don't flush yet!
        cache1.PopAnchor(Tree::empty_root(), type);

        {
            CCoinsViewCacheTest cache2(&cache1); // Build cache on top
            cache2.PushAnchor(tree); // Put the same anchor back!
            cache2.Flush(); // Flush to cache1
        }

        // cache2's flush kinda worked, i.e. cache1 thinks the
        // tree is there, but it didn't bring down the correct
        // treestate...
        {
            Tree checktree;
            BOOST_CHECK(GetAnchorAt(cache1, tree.root(), checktree));
            BOOST_CHECK(checktree.root() == tree.root()); // Oh, shucks.
        }

        // Flushing cache won't help either, just makes the inconsistency
        // permanent.
        cache1.Flush();
        {
            Tree checktree;
            BOOST_CHECK(GetAnchorAt(cache1, tree.root(), checktree));
            BOOST_CHECK(checktree.root() == tree.root()); // Oh, shucks.
        }
    }
}

BOOST_AUTO_TEST_CASE(anchor_pop_regression_test)
{
    BOOST_TEST_CONTEXT("Sprout") {
        anchorPopRegressionTestImpl<SproutMerkleTree>(SPROUT);
    }
    BOOST_TEST_CONTEXT("Sapling") {
        anchorPopRegressionTestImpl<SaplingMerkleTree>(SAPLING);
    }
}

template<typename Tree> void anchorRegressionTestImpl(ShieldedType type)
{
    // Correct behavior:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        Tree tree;
        tree.append(GetRandHash());

        cache1.PushAnchor(tree);
        cache1.Flush();

        cache1.PopAnchor(Tree::empty_root(), type);
        BOOST_CHECK(cache1.GetBestAnchor(type) == Tree::empty_root());
        BOOST_CHECK(!GetAnchorAt(cache1, tree.root(), tree));
    }

    // Also correct behavior:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        Tree tree;
        tree.append(GetRandHash());
        cache1.PushAnchor(tree);
        cache1.Flush();

        cache1.PopAnchor(Tree::empty_root(), type);
        cache1.Flush();
        BOOST_CHECK(cache1.GetBestAnchor(type) == Tree::empty_root());
        BOOST_CHECK(!GetAnchorAt(cache1, tree.root(), tree));
    }

    // Works because we bring the anchor in from parent cache.
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        Tree tree;
        tree.append(GetRandHash());
        cache1.PushAnchor(tree);
        cache1.Flush();

        {
            // Pop anchor.
            CCoinsViewCacheTest cache2(&cache1);
            BOOST_CHECK(GetAnchorAt(cache2, tree.root(), tree));
            cache2.PopAnchor(Tree::empty_root(), type);
            cache2.Flush();
        }

        BOOST_CHECK(cache1.GetBestAnchor(type) == Tree::empty_root());
        BOOST_CHECK(!GetAnchorAt(cache1, tree.root(), tree));
    }

    // Was broken:
    {
        CCoinsViewTest base;
        CCoinsViewCacheTest cache1(&base);

        // Insert anchor into base.
        Tree tree;
        tree.append(GetRandHash());
        cache1.PushAnchor(tree);
        cache1.Flush();

        {
            // Pop anchor.
            CCoinsViewCacheTest cache2(&cache1);
            cache2.PopAnchor(Tree::empty_root(), type);
            cache2.Flush();
        }

        BOOST_CHECK(cache1.GetBestAnchor(type) == Tree::empty_root());
        BOOST_CHECK(!GetAnchorAt(cache1, tree.root(), tree));
    }
}

BOOST_AUTO_TEST_CASE(anchor_regression_test)
{
    BOOST_TEST_CONTEXT("Sprout") {
        anchorRegressionTestImpl<SproutMerkleTree>(SPROUT);
    }
    BOOST_TEST_CONTEXT("Sapling") {
        anchorRegressionTestImpl<SaplingMerkleTree>(SAPLING);
    }
}

BOOST_AUTO_TEST_CASE(nullifiers_test)
{
    CCoinsViewTest base;
    CCoinsViewCacheTest cache(&base);

    TxWithNullifiers txWithNullifiers;
    checkNullifierCache(cache, txWithNullifiers, false);
    cache.SetNullifiers(txWithNullifiers.tx, true);
    checkNullifierCache(cache, txWithNullifiers, true);
    cache.Flush();

    CCoinsViewCacheTest cache2(&base);

    checkNullifierCache(cache2, txWithNullifiers, true);
    cache2.SetNullifiers(txWithNullifiers.tx, false);
    checkNullifierCache(cache2, txWithNullifiers, false);
    cache2.Flush();

    CCoinsViewCacheTest cache3(&base);

    checkNullifierCache(cache3, txWithNullifiers, false);
}

template<typename Tree> void anchorsFlushImpl(ShieldedType type)
{
    CCoinsViewTest base;
    uint256 newrt;
    {
        CCoinsViewCacheTest cache(&base);
        Tree tree;
        BOOST_CHECK(GetAnchorAt(cache, cache.GetBestAnchor(type), tree));
        tree.append(GetRandHash());

        newrt = tree.root();

        cache.PushAnchor(tree);
        cache.Flush();
    }
    
    {
        CCoinsViewCacheTest cache(&base);
        Tree tree;
        BOOST_CHECK(GetAnchorAt(cache, cache.GetBestAnchor(type), tree));

        // Get the cached entry.
        BOOST_CHECK(GetAnchorAt(cache, cache.GetBestAnchor(type), tree));

        uint256 check_rt = tree.root();

        BOOST_CHECK(check_rt == newrt);
    }
}

BOOST_AUTO_TEST_CASE(anchors_flush_test)
{
    BOOST_TEST_CONTEXT("Sprout") {
        anchorsFlushImpl<SproutMerkleTree>(SPROUT);
    }
    BOOST_TEST_CONTEXT("Sapling") {
        anchorsFlushImpl<SaplingMerkleTree>(SAPLING);
    }
}

BOOST_AUTO_TEST_CASE(chained_joinsplits)
{
    // TODO update this or add a similar test when the SaplingNote class exist
    CCoinsViewTest base;
    CCoinsViewCacheTest cache(&base);

    SproutMerkleTree tree;

    JSDescription js1;
    js1.anchor = tree.root();
    js1.commitments[0] = appendRandomSproutCommitment(tree);
    js1.commitments[1] = appendRandomSproutCommitment(tree);

    // Although it's not possible given our assumptions, if
    // two joinsplits create the same treestate twice, we should
    // still be able to anchor to it.
    JSDescription js1b;
    js1b.anchor = tree.root();
    js1b.commitments[0] = js1.commitments[0];
    js1b.commitments[1] = js1.commitments[1];

    JSDescription js2;
    JSDescription js3;

    js2.anchor = tree.root();
    js3.anchor = tree.root();

    js2.commitments[0] = appendRandomSproutCommitment(tree);
    js2.commitments[1] = appendRandomSproutCommitment(tree);

    js3.commitments[0] = appendRandomSproutCommitment(tree);
    js3.commitments[1] = appendRandomSproutCommitment(tree);

    {
        CMutableTransaction mtx;
        mtx.vjoinsplit.push_back(js2);

        BOOST_CHECK(!cache.HaveJoinSplitRequirements(mtx));
    }

    {
        // js2 is trying to anchor to js1 but js1
        // appears afterwards -- not a permitted ordering
        CMutableTransaction mtx;
        mtx.vjoinsplit.push_back(js2);
        mtx.vjoinsplit.push_back(js1);

        BOOST_CHECK(!cache.HaveJoinSplitRequirements(mtx));
    }

    {
        CMutableTransaction mtx;
        mtx.vjoinsplit.push_back(js1);
        mtx.vjoinsplit.push_back(js2);

        BOOST_CHECK(cache.HaveJoinSplitRequirements(mtx));
    }

    {
        CMutableTransaction mtx;
        mtx.vjoinsplit.push_back(js1);
        mtx.vjoinsplit.push_back(js2);
        mtx.vjoinsplit.push_back(js3);

        BOOST_CHECK(cache.HaveJoinSplitRequirements(mtx));
    }

    {
        CMutableTransaction mtx;
        mtx.vjoinsplit.push_back(js1);
        mtx.vjoinsplit.push_back(js1b);
        mtx.vjoinsplit.push_back(js2);
        mtx.vjoinsplit.push_back(js3);

        BOOST_CHECK(cache.HaveJoinSplitRequirements(mtx));
    }
}

template<typename Tree> void anchorsTestImpl(ShieldedType type)
{
    // TODO: These tests should be more methodical.
    //       Or, integrate with Bitcoin's tests later.

    CCoinsViewTest base;
    CCoinsViewCacheTest cache(&base);

    BOOST_CHECK(cache.GetBestAnchor(type) == Tree::e