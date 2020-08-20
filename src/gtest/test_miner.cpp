#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chainparams.h"
#include "key.h"
#include "miner.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <boost/optional.hpp>

using ::testing::Return;

#ifdef ENABLE_WALLET
class MockReserveKey : public CReserveKey {
public:
    MockReserveKey() : CReserveKey(nullptr) { }

    MOCK_METHOD1(GetReservedKey, bool(CPubKey &pubkey));
};
#endif

TEST(Miner, GetMinerScriptPubKey) {
    SelectParams