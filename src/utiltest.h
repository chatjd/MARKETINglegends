
// Copyright (c) 2016 The Zcash developers
// Copyright (c) 2017-2018 The SnowGem developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/wallet.h"
#include "vidulum/JoinSplit.hpp"
#include "vidulum/Note.hpp"
#include "vidulum/NoteEncryption.hpp"

CWalletTx GetValidReceive(ZCJoinSplit& params,
                          const libzcash::SproutSpendingKey& sk, CAmount value,
                          bool randomInputs,
                          int32_t version = 2);
libzcash::SproutNote GetNote(ZCJoinSplit& params,
                       const libzcash::SproutSpendingKey& sk,
                       const CTransaction& tx, size_t js, size_t n);
CWalletTx GetValidSpend(ZCJoinSplit& params,
                        const libzcash::SproutSpendingKey& sk,
                        const libzcash::SproutNote& note, CAmount value);