
/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "algebra/curves/alt_bn128/alt_bn128_pp.hpp"

namespace libsnark {

void alt_bn128_pp::init_public_params()
{
    init_alt_bn128_params();