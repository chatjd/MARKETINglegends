// Copyright (c) 2017 The Zcash developers
// Copyright (c) 2017-2018 The SnowGem developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amqppublishnotifier.h"
#include "main.h"
#include "util.h"

#include "amqpsender.h"

#include <memory>
#include <thread>

static std::multimap<std::string, AMQPAbstractPublishNotifier*> mapPublishNotifiers;

static const char *MSG_HASHBLOCK = "hashblock";
static const char *MSG_HASHTX    = "hashtx";
static const char *MSG_RAWBLOCK  = "rawblock";
static const char *MSG_RAWTX     = "rawtx";

// Invoke this method from a new thread to run the proton container event loop.
void AM