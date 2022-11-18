// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zip32.h"

#include "hash.h"
#include "random.h"
#include "streams.h"
#include "version.h"
#include "vidulum/prf.h"

#include <librustzcash.h>
#include <sodium.h>

const unsigned char VIDULUM_HD_SEED_FP_PERSONAL[crypto_generichash_blake2b_PERSONALBYTES] =
    {'Z', 'c', 'a', 's', 'h', '_', 'H', 'D', '_', 'S', 'e', 'e', 'd', '_', 'F', 'P'};

const unsigned char VIDULUM_TADDR_OVK_PERSONAL[crypto_generichash_blake2b_PERSONALBYTES] =
    {'Z', 'c', 'T', 'a', 'd', 'd', 'r', 'T', 'o', 'S', 'a', 'p', 'l', 'i', 'n', 'g'};

HDSeed HDSeed::Random(size_t len)
{
    assert(len >= 32);
    RawHDSeed rawSeed(len, 0);
    GetRandBytes(rawSeed.data(), len);
    return HDSeed(rawSeed);
}

uint256 HDSeed::Fingerprint() const
{
    CBLAKE2bWriter h(SER_GETHASH, 0, VIDULUM_HD_SEED_FP_PERSONAL);
    h << seed;
    return h.GetHash();
}

uint256 ovkForShieldingFromTaddr(HDSeed& seed) {
    auto rawSeed = seed.RawSeed();

    // I = BLAKE2b-512("ZcTaddrToSapling", seed)
    crypto_generichash_blake2b_state state;
    assert(crypto_generichash_blake2b_init_salt_personal(
        &state,
        NULL, 0, // No key.
        64,
        NULL,    // No salt.
        VIDULUM_TADDR_OVK_PERSONAL) == 0);
    crypto_generichash_blake2b_update(&state, rawSeed.data(), rawSeed.size());
    auto intermediate = std::array<unsigned char, 64>();
    crypto_generichash_blake2b_final(&state, intermediate.data(), 64);

    // I_L = I[0..32]
    uint256 intermediate_L;
    memcpy(intermediate_L.begin(), in