/** @file
 *****************************************************************************

 Implementation of interfaces for auxiliary gadgets for the SHA256 gadget.

 See sha256_aux.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SHA256_AUX_TCC_
#define SHA256_AUX_TCC_

namespace libsnark {

template<typename FieldT>
lastbits_gadget<FieldT>::lastbits_gadget(protoboard<FieldT> &pb,
                                         const pb_variable<FieldT> &X,
                                         const size_t X_bits,
                                         const pb_variable<FieldT> &result,
                                         const pb_linear_combination_array<FieldT> &result_bits,
                                         const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    X(X),
    X_bits(X_bits),
    result(result),
    result_bits(result_bits)
{
    full_bits = result_bits;
    for (size_t i = result_bits.size(); i < X_bits; ++i)
    {
        pb_variable<FieldT> full_bits_overflow;
        full_bits_overflow.allocate(pb, FMT(this->annotation_prefix, " full_bits_%zu", i));
        full_bits.emplace_back(full_bits_overflow);
    }

    unpack_bits.reset(new packing_gadget<FieldT>(pb, full_bits, X, FMT(this->annotation_prefix, " unpack_bits")));
    pack_result.reset(new packing_gadget<FieldT>(pb, result_bits, result, FMT(this->annotation_prefix, " pack_result")));
}

template<typename FieldT>
void lastbits_gadget<FieldT>::generate_r1cs_constraints()
{
    unpack_bits->generate_r1cs_constraints(true);
    pack_result->generate_r1cs_constraints(false);
}

template<typename FieldT>
void lastbits_gadget<FieldT>::generate_r1cs_witness()
{
    unpack_bits->generate_r1cs_witness_from_packed();
    pack_result->generate_r1cs_witness_from_bits();
}

template<typename FieldT>
XOR3_gadget<FieldT>::XOR3_gadget(protoboard<FieldT> &pb,
                                 const pb_linear_combination<FieldT> &A,
                                 const pb_linear_combination<FieldT> &B,
                                 const pb_linear_combination<FieldT> &C,
                                 const bool assume_C_is_zero,
                                 const pb_linear_combination<FieldT> &out,
                                 const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    A(A),
    B(B),
    C(C),
    assume_C_is_zero(assume_C_is_zero),
    out(out)
{
    if (!assume_C_is_zero)
    {
        tmp.allocate(pb, FMT(this->annotation_prefix, " tmp"));
    }
}

template<typename FieldT>
void XOR3_gadget<FieldT>::generate_r1cs_constraints()
{
    /*
      tmp = A + B - 2AB i.e. tmp = A xor B
      out = tmp + C - 2tmp C i.e. out = tmp xor C
    */
    if (assume_C_is_zero)
    {
        