/** @file
 *****************************************************************************

 Declaration of interfaces for the Merkle tree check read gadget.

 The gadget checks the following: given a root R, address A, value V, and
 authentication path P, check that P is a valid authentication path for the
 value V as the A-th leaf in a Merkle tree with root R.

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef MERKLE_TREE_CHECK_READ_GADGET_HPP_
#define MERKLE_TREE_CHECK_READ_GADGET_HPP_

#include "common/data_structures/merkle_tree.hpp"
#include "gadgetlib1/gadget.hpp"
#include "gadgetlib1/gadgets/hashes/hash_io.hpp"
#include "gadgetlib1/gadgets/has