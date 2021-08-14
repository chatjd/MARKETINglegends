
/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef PB_VARIABLE_TCC_
#define PB_VARIABLE_TCC_
#include <cassert>
#include "gadgetlib1/protoboard.hpp"
#include "common/utils.hpp"

namespace libsnark {

template<typename FieldT>
void pb_variable<FieldT>::allocate(protoboard<FieldT> &pb, const std::string &annotation)
{
    this->index = pb.allocate_var_index(annotation);
}

/* allocates pb_variable<FieldT> array in MSB->LSB order */
template<typename FieldT>
void pb_variable_array<FieldT>::allocate(protoboard<FieldT> &pb, const size_t n, const std::string &annotation_prefix)
{
#ifdef DEBUG
    assert(annotation_prefix != "");
#endif
    (*this).resize(n);

    for (size_t i = 0; i < n; ++i)
    {
        (*this)[i].allocate(pb, FMT(annotation_prefix, "_%zu", i));
    }
}

template<typename FieldT>
void pb_variable_array<FieldT>::fill_with_field_elements(protoboard<FieldT> &pb, const std::vector<FieldT>& vals) const
{
    assert(this->size() == vals.size());
    for (size_t i = 0; i < vals.size(); ++i)
    {
        pb.val((*this)[i]) = vals[i];