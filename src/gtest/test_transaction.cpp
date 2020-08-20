#include <gtest/gtest.h>

#include "primitives/transaction.h"
#include "vidulum/Note.hpp"
#include "vidulum/Address.hpp"

#include <array>

extern ZCJoinSplit* params;
extern int GenZero(int n);
extern int GenMax(int n);

TEST(Transaction, JSDescriptionRandomized) {
    // construct a merkle tree
    SproutMerkleTree merkleTree;

    libzcash::SproutSpendingKey k = libzcash::SproutSpendingKey::random();
    libzcash::SproutPaymentAddress addr = k.address()