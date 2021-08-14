/** @file
 *****************************************************************************

 Implementation of interfaces for the Merkle tree check read.

 See merkle_tree_check_read_gadget.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef MERKLE_TREE_CHECK_READ_GADGET_TCC_
#define MERKLE_TREE_CHECK_READ_GADGET_TCC_

namespace libsnark {

template<typename FieldT, typename HashT>
merkle_tree_check_read_gadget<FieldT, HashT>::merkle_tree_check_read_gadget(protoboard<FieldT> &pb,
                                                                            const size_t tree_depth,
                                                                            const pb_linear_combination_array<FieldT> &address_bits,
                                                                            const digest_variable<FieldT> &leaf,
                                                                            const digest_variable<FieldT> &root,
                                                                            const merkle_authentication_path_variable<FieldT, HashT> &path,
                                                                            const pb_linear_combination<FieldT> &read_successful,
                                                                            const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    digest_size(HashT::get_digest_len()),
    tree_depth(tree_depth),
    address_bits(address_bits),
    leaf(leaf),
    root(root),
    path(path),
    read_successful(read_successful)
{
    /*
       The tricky part here is ordering. For Merkle tree
       authentication paths, path[0] corresponds to one layer below
       the root (and path[tree_depth-1] corresponds to the layer
       containing the leaf), while address_bits has the reverse order:
       address_bits[0] is LSB, and corresponds to layer containing the
       leaf, and address_bits[tree_depth-1] is MSB, and corresponds to
       the subtree directly under the root.
    */
    assert(tree_depth > 0);
    assert(tree_depth == address_bits.size());

    for (size_t i = 0; i < tree_depth-1; ++i)
    {
        internal_output.emplace_back(digest_variable<FieldT>(pb, digest_size, FMT(this->annotation_prefix, " internal_output_%zu", i)));
    }

    computed_root.reset(new digest_variable<FieldT>(pb, digest_size, FMT(this->annotation_prefix, " computed_root")));

    for (size_t i = 0; i < tree_depth; ++i)
    {
        block_variable<FieldT> inp(pb, path.left_digests[i], path.right_digests[i], FMT(this->annotation_prefix, " inp_%zu", i));
        hasher_inputs.emplace_back(inp);
        hashers.emplace_back(HashT(pb, 2*digest_size, inp, (i == 0 ? *computed_root : internal_output[i-1]),
                                   FMT(this->annotation_prefix, " load_hashers_%zu", i)));
    }

    for (size_t i = 0; i < tree_depth; ++i)
    {
        /*
          The propagators take a computed hash value (or leaf in the
          base case) and propagate it one layer up, either in the left
          or the right slot of authentication_path_variable.
        */
        propagators.emplace_back(digest_selector_gadget<FieldT>(pb, digest_size, i < tree_depth - 1 ? internal_output[i] : leaf,
                                                                address_bits[tree_depth-1-i], path.left_digests[i], path.right_digests[i],
                                                                FMT(this->annotation_prefix, " digest_selector_%zu", i)));
    }

    check_root.reset(new bit_vector_copy_gadget<FieldT>(pb, computed_root->bits, root.bits, read_successful, FieldT::capacity(), FMT(annotation_prefix, " check_root")));
}

template<typename FieldT, typename HashT>
void merkle_tree_check_read_gadget<FieldT, HashT>::generate_r1cs_constraints()
{
    /* ensure correct hash computations */
    for (size_t i = 0; i < tree_depth; ++i)
    {
        // Note that we check root outside and have enforced booleanity of path.left_di