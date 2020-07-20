// Copyright (c) 2016 The Zcash developers
// Copyright (c) 2017-2018 The SnowGem developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "asyncrpcqueue.h"

static std::atomic<size_t> workerCounter(0);

/**
 * Static method to return the shared/default queue.
 */
shared_ptr<AsyncRPCQueue> AsyncRPCQueue::shar