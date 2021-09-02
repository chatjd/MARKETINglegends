// Copyright (c) 2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//
// Unit tests for alert system
//

#include "alert.h"
#include "chain.h"
#include "chainparams.h"
#include "clientversion.h"
#include "data/alertTests.raw.h"

#include "main.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "serialize.h"
#include "streams.h"
#