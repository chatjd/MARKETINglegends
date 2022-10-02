// Copyright (c) 2012-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"

#include "chainparams.h"
#include "key_io.h"
#include "script/script.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"
#include "test/test_bitcoin.h"

#include "vidulum/Address.hpp"

#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace libzcash;

static const std::string strSecret1 = "5HxWvvfubhXpYYpS3tJkw6fq9jE9j18THftkZjHHfmFiWtmAbrj";
static const std::string strSecret2 = "5KC4ejrDjv152FGwP386VD1i2NYc5KkfSMyv1nGy1VGDxGHqVY3";
static const std::string strSecret1C = "Kwr371tjA9u2rFSMZjTNun2PXXP3WPZu2afRHTcta6KxEUdm1vEw";
static const std::string strSecret2C = "L3Hq7a8FEQwJkW1M2GNKDW28546Vp5miewcCzSqUD9kCAXrJdS3g";
static const std::string addr1 = "t1h8SqgtM3QM5e2M8EzhhT1yL2PXXtA6oqe";
static const std::string addr2 = "t1Xxa5ZVPKvs9bGMn7aWTiHjyHvR31XkUst";
static const std::string addr1C = "t1ffus9J1vhxvFqLoExGBRPjE7BcJxiSCTC";
static cons