/** @file
 *****************************************************************************

 Implementation of interfaces for a gadget that can be created from an R1CS constraint system.

 See gadget_from_r1cs.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef GADGET_FROM_R1CS_TCC_
#define GADGET_FROM_R1CS_TCC_

namespace libsnark {

template<typename FieldT>
gadget_from_r1cs<FieldT>::gadget_from_r1cs(protoboard<FieldT> &pb,
                                           const std::vector<pb_variable_array<FieldT> > &vars,
                                           const r1cs_constraint_system<FieldT> &cs,
                                           const std::string &annotation_prefix) :
    gadget<FieldT>(pb, annotation_prefix),
    vars(vars),
    cs(cs)
{
    cs_to_vars[0] = 0; /* constant term maps to constant term */

    size_t cs_var_idx = 1;
    for (auto va : vars)
    {
#ifdef DEBUG
        printf("gadget_from_r1cs: translating a block of variables with length %zu\n", va.size());
#endif
        for (auto v : va)
        {
            cs_to_vars[cs_var_idx] = v.index;

#ifdef DEBUG
            if (v.index != 0)
            {
                // handle annotations, except for re-annotating constant term
                const std::map<size_t, std::string>::const_iterator it = cs.variable_annotations.find(cs_var_idx);

                s