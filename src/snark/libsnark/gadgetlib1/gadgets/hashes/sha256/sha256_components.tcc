/** @file
 *****************************************************************************

 Implementation of interfaces for gadgets for the SHA256 message schedule and round function.

 See sha256_components.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SHA256_COMPONENTS_TCC_
#define SHA256_COMPONENTS_TCC_

namespace libsnark {

const uint32_t SHA256_K[64] =  {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

const uint32_t SHA256_H[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

template<typename FieldT>
pb_linear_combination_array<FieldT> SHA256_default_IV(protoboard<FieldT> &pb)
{
    pb_linear_combination_array<FieldT> result;
    result.reserve(SHA256_digest_size);

    for (size_t i = 0; i < SHA256_digest_size; ++i)
    {
        int iv_val = (SHA256_H[i / 32] >> (31-(i % 32))) & 1;

        pb_linear_combination<FieldT> iv_element;
        iv_element.assign(pb, iv_val * ONE);
        iv_element.evaluate(pb);

        result.emplace_back(iv_element);
    }

    return result;
}

template<typename FieldT>
sha256_message_schedule_gadget<FieldT>::sha256_message_schedule_gadget(protoboard<FieldT> &pb,
                                                                       const pb_variable_array<FieldT> &M,
                                                                       const pb_variable_array<FieldT> &packed_W,
                                                                       const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    M(M),
    packed_W(packed_W)
{
    W_bits.resize(64);

    pack_W.resize(16);
    for (size_t i = 0; i < 16; ++i)
    {
        W_bits[i] = pb_variable_array<FieldT>(M.rbegin() + (15-i) * 32, M.rbegin() + (16-i) * 32);
        pack_W[i].reset(new packing_gadget<FieldT>(pb, W_bits[i], packed_W[i], FMT(this->annotation_prefix, " pack_W_%zu", i)));
    }

    /* NB: some of those will be un-allocated */
    sigma0.resize(64);
    sigma1.resize(64);
    compute_sigma0.resize(64);
    compute_sigma1.resize(64);
    unreduced_W.resize(64);
    mod_reduce_W.resize(64);

    for (size_t i = 16; i < 64; ++i)
    {
        /* allocate result variables for sigma0/sigma1 invocations */
        sigma0[i].allocate(pb, FMT(this->annotation_prefix, " sigma0_%zu", i));
        sigma1[i].allocate(pb, FMT(this->annotation_prefix, " sigma1_%zu", i));

        /* compute sigma0/sigma1 */
        compute_sigma0[i].reset(new small_sigma_gadget<FieldT>(pb, W_bits[i-15], sigma0[i], 7, 18, 3, FMT(this->annotation_prefix, " compute_sigma0_%zu", i)));
        compute_sigma1[i].reset(new small_sigma_gadget<FieldT>(pb, W_bits[i-2], sigma1[i], 17, 19, 10, FMT(this->annotation_prefix, 