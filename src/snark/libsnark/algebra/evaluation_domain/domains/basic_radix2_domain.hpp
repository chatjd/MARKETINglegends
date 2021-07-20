/** @file
 *****************************************************************************

 Declaration of interfaces for the "basic radix-2" evaluation domain.

 Roughly, the domain has size m = 2^k and consists of the m-th roots of unity.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef BASIC_RADIX2_DOMAIN_HPP_
#define BASIC_RADIX2_DOMAIN_HPP_

#include "algebra/evaluation_domain/evaluation_domain.hpp"

namespace libsnark {

template<typename FieldT>
class basic_radix2_domain : public evaluation_domain<Fie