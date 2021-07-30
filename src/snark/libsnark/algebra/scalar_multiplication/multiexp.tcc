/** @file
 *****************************************************************************

 Implementation of interfaces for multi-exponentiation routines.

 See multiexp.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef MULTIEXP_TCC_
#define MULTIEXP_TCC_

#include "algebra/fields/fp_aux.tcc"

#include <algorithm>
#include <cassert>
#include <type_traits>

#include "common/profiling.hpp"
#include "common/utils.hpp"
#include "algebra/scalar_multiplication/wnaf.hpp"

namespace libsnark {

template<mp_size_t n>
class ordered_exponent {
// to use std::push_heap and friends later
public:
    size_t idx;
    bigint<n> r;

    ordered_exponent(const size_t idx, const bigint<n> &r) : idx(idx), r(r) {};

    bool operator<(const ordered_exponent<n> &other) const
    {
#if defined(__x86_64__) && defined(USE_ASM)
        if (n == 3)
        {
            int64_t res;
            _