#include <gtest/gtest.h>

#include "vidulum/Address.hpp"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "util.h"

#include <boost/filesystem.hpp>

/**
 * This test covers Sapling methods on CWallet
 * GenerateNewSaplingZKey()
 * AddSaplingZKey()
 * AddSaplingIncomingViewingKey()
 * LoadSaplingZKey()
 * LoadSaplingIncomingViewingKey()
 * LoadSaplingZKeyMetadata()
 */
TEST(wallet_zkeys_tests, StoreAndLoadSaplingZkeys) {
    SelectParams(CBaseChainParams::MAIN);

    CWallet wallet;

    // wallet should be empty
    std::set<libzcash::SaplingPaymentAddress> addrs;
    wallet.GetSaplingPaymentAddresses(addrs);
    ASSERT_EQ(0, addrs.size());

    // No HD seed in the wallet
    EXPECT_ANY_THROW(wallet.GenerateNewSaplingZKey());

    // Load the all-zeroes seed
    CKeyingMaterial rawSeed(32, 0);
    HDSeed seed(rawSeed);
    wallet.LoadHDSeed(seed);

    // Now this call succeeds
    auto address = wallet.GenerateNewSaplingZKey();

    // wallet should have one key
    wallet.GetSaplingPaymentAddresses(addrs);
    ASSERT_EQ(1, addrs.size());
    
    // verify wallet has incoming viewing key for the address
    ASSERT_TRUE(wallet.HaveSaplingIncomingViewingKey(address));
    
    // manually add new spending key to wallet
    auto m = libzcash::SaplingExtendedSpendingKey::Master(seed);
    auto sk = m.Derive(0);
    ASSERT_TRUE(wallet.AddSaplingZKey(sk, sk.DefaultAddress()));

    // verify wallet did add it
    auto fvk = sk.expsk.full_viewing_key();
    ASSERT_TRUE(wallet.HaveSaplingSpendingKey(fvk));

    // verify spending key stored correctly
    libzcash::SaplingExtendedSpendingKey keyOut;
    wallet.GetSaplingSpendingKey(fvk, keyOut);
    ASSERT_EQ(sk, keyOut);

    // verify there are two keys
    wallet.GetSaplingPaymentAddresses(addrs);
    EXPECT_EQ(2, addrs.size());
    EXPECT_EQ(1, addrs.count(address));
    EXPECT_EQ(1, addrs.count(sk.DefaultAddress()));

    // Generate a diversified address different to the default
    // 