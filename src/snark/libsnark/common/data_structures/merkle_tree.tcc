/** @file
 *****************************************************************************

 Implementation of interfaces for Merkle tree.

 See merkle_tree.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef MERKLE_TREE_TCC
#define MERKLE_TREE_TCC

#include <algorithm>

#include "common/profiling.hpp"
#include "common/utils.hpp"

namespace libsnark {

template<typename HashT>
typename HashT::hash_value_type two_to_one_CRH(const typename HashT::hash_value_type &l,
                                               const typename HashT::hash_value_type &r)
{
    typename HashT::hash_value_type new_input;
    new_input.insert(new_input.end(), l.begin(), l.end());
    new_input.insert(new_input.end(), r.begin(), r.end());

    const size_t digest_size = HashT::get_digest_len();
 