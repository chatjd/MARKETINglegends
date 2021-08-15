/** @file
*****************************************************************************

Implementation of interfaces for a QAP ("Quadratic Arithmetic Program").

See qap.hpp .

*****************************************************************************
* @author     This file is part of libsnark, developed by SCIPR Lab
*             and contributors (see AUTHORS).
* @copyright  MIT license (see LICENSE file)
*****************************************************************************/

#ifndef QAP_TCC_
#define QAP_TCC_

#include "common/profiling.hpp"
#include "common/utils.hpp"
#include "algebra/evaluation_domain/evaluation_domain.hpp"
#include "algebra/scalar_multiplication/multiexp.hpp"

namespace libsnark {

template<typename FieldT>
qap_instance<FieldT>::qap_instance(const std::shared_ptr<evaluation_domain<FieldT> > &domain,
                                   const size_t num_variables,
                                   const size_t degree,
                                   const size_t num_inputs,
                                   const std::vector<std::map<size_t, FieldT> > &A_in_Lagrange_basis,
                                   const std::vector<std::map<size_t, FieldT> > &B_in_Lagrange_basis,
                                   const std::vector<std::map<size_t, FieldT> > &C_in_Lagrange_basis) :
    num_variables_(num_variables),
    degree_(degree),
    num_inputs_(num_inputs),
    domain(domain),
    A_in_Lagrange_basis(A_in_Lagrange_basis),
    B_in_Lagrange_basis(B_in_Lagrange_basis),
    C_in_Lagrange_basis(C_in_Lagrange_basis)
{
}

template<typename FieldT>
qap_instance<FieldT>::qap_instance(const std::shared_ptr<evaluation_domain<FieldT> > &domain,
                                   const size_t num_variables,
                                   const size_t degree,
                                   const size_t num_inputs,
                                   std::vector<std::map<size_t, FieldT> > &&A_in_Lagrange_basis,
                                   std::vector<std::map<size_t, FieldT> > &&B_in_Lagrange_basis,
                                   std::vector<std::map<size_t, FieldT> > &&C_in_Lagrange_basis) :
    num_variables_(num_variables),
    degree_(degree),
    num_inputs_(num_inputs),
    domain(domain),
    A_in_Lagrange_basis(std::move(A_in_Lagrange_basis)),
    B_in_Lagrange_basis(std::move(B_in_Lagrange_basis)),
    C_in_Lagrange_basis(std::move(C_in_Lagrange_basis))
{
}

template<typename FieldT>
size_t qap_instance<FieldT>::num_variables() const
{
    return num_variables_;
}

template<typename FieldT>
size_t qap_instance<FieldT>::degree() const
{
    return degree_;
}

template<typename FieldT>
size_t qap_instance<FieldT>::num_inputs() const
{
    return num_inputs_;
}

template<typename FieldT>
bool qap_instance<FieldT>::is_satisfied(const qap_witness<FieldT> &witness) const
{
    const FieldT t = FieldT::random_element();

    std::vector<FieldT> At(this->num_variables()+1, FieldT::zero());
    std::vector<FieldT> Bt(this->num_variables()+1, FieldT::zero());
    std::vector<FieldT> Ct(this->num_variables()+1, FieldT::zero());
    std::vector<FieldT> Ht(this->degree()+1);

    const FieldT Zt = this->domain->compute_Z(t);

    const std::vector<FieldT> u = this->domain->lagrange_coeffs(t);

    for (size_t i = 0; i < this->num_variables()+1; ++i)
    {
        for (auto &el : A_in_Lagrange_basis[i])
        {
            At[i] += u[el.first] * el.second;
        }

        for (auto &el : B_in_Lagrange_basis[i])
        {
            Bt[i] += u[el.first] * el.second;
        }

        for (auto &el : C_in_Lagrange_basis[i])
        {
            Ct[i] += u[el.first] * el.second;
        }
    }

    FieldT ti = FieldT::one();
    for (size_t i = 0; i < this->degree()+1; ++i)
    {
        Ht[i] = ti;
        ti *= t;
    }

    const qap_instance_evaluation<FieldT> eval_qap_inst(this->domain,
                                                        this->num_variables(),
                                                        this->degree(),
                                                        this->num_inputs(),
                                                        t,
                                                        std::move(At),
                                                        std::move(Bt),
                                                        std::move(Ct),
                                                        std::move(Ht),
                                                        Zt);
    return eval_qap_inst.is_satisfied(witness);
}

template<typename FieldT>
qap_instance_evaluation<FieldT>::qap_instance_evaluation(const std::shared_ptr<evaluation_domain<FieldT> > &domain,
                                                         const size_t num_variables,
                                                         const size_t degree,
                                                         const size_t num_inputs,
                                                         const FieldT &t,
                                                         const std::vector<FieldT> &At,
                                                         const std::vector<FieldT> &Bt,
                                                         const std::vector<FieldT> &Ct,
                                                         const std::vector<FieldT> &Ht,
                                                         const FieldT &Zt) :
    num_variables_(num_variables),
    degree_(degree),
    num_inputs_(num_inputs),
    domain(domain),
    t(t),
    At(At),
    Bt(Bt),
    Ct(Ct),
    Ht(Ht),
    Zt(Zt)
{
}

template<typename FieldT>
qap_instance_evaluation<FieldT>::qap_instance_evaluation(const std::shared_ptr<evaluation_domain<FieldT> > &domain,
                                                         const size_t 