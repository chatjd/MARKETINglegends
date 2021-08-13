/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SIMPLE_EXAMPLE_TCC_
#define SIMPLE_EXAMPLE_TCC_

#include <cassert>
#include "gadgetlib1/gadgets/basic_gadgets.hpp"

namespace libsnark {

/* NOTE: all examples here actually generate one constraint less to account for soundness constraint in QAP */

template<typename FieldT>
r1cs_example<FieldT> gen_r1cs_example_from_protoboard(const size_t num_constraints,
                                                     