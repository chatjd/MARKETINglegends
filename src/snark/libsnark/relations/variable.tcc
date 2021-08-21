/** @file
 *****************************************************************************

 Implementation of interfaces for:
 - a variable (i.e., x_i),
 - a linear term (i.e., a_i * x_i), and
 - a linear combination (i.e., sum_i a_i * x_i).

 See variabe.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef VARIABLE_TCC_
#define VARIABLE_TCC_

#include <algorithm>
#include <cassert>

#include "algebra/fields/bigint.hpp"

namespace libsnark {

template<typename FieldT>
linear_term<FieldT> variable<FieldT>::operator*(const integer_coeff_t int_coeff) const
{
    return linear_term<FieldT>(*this, int_coeff);
}

template<typename FieldT>
linear_term<FieldT> variable<FieldT>::operator*(const FieldT &field_coeff) const
{
    return linear_term<FieldT>(*this, field_coeff);
}

template<typename FieldT>
linear_combination<FieldT> variable<FieldT>::operator+(const linear_combination<FieldT> &other) const
{
    linear_combination<FieldT> result;

    result.add_term(*this);
    result.terms.insert(result.terms.begin(), other.terms.begin(), other.terms.end());

    return result;
}

template<typename FieldT>
linear_combination<FieldT> variable<FieldT>::operator-(const linear_combination<FieldT> &other) const
{
    return (*this) + (-other);
}

template<typename FieldT>
linear_term<FieldT> variable<FieldT>::operator-() const
{
    return linear_term<FieldT>(*this, -FieldT::one());
}

template<typename FieldT>
bool variable<FieldT>::operator==(const variable<FieldT> &other) const
{
    return (this->index == other.index);
}

template<typename FieldT>
linear_term<FieldT> operator*(const integer_coeff_t int_coeff, const variable<FieldT> &var)
{
    return linear_term<FieldT>(var, int_coeff);
}

template<typename FieldT>
linear_term<FieldT> operator*(const FieldT &field_coeff, const variable<FieldT> &var)
{
    return linear_term<FieldT>(var, field_coeff);
}

template<typename FieldT>
linear_combination<FieldT> operator+(const integer_coeff_t int_coeff, const variable<FieldT> &var)
{
    return linear_combination<FieldT>(int_coeff) + var;
}

template<typename FieldT>
linear_combination<FieldT> operator+(const FieldT &field_coeff, const variable<FieldT> &var)
{
    return linear_combination<FieldT>(field_coeff) + var;
}

template<typename FieldT>
linear_combination<FieldT> operator-(const integer_coeff_t int_coeff, const variable<FieldT> &var)
{
    return linear_combination<FieldT>(int_coeff) - var;
}

template<typename FieldT>
linear_combination<FieldT> operator-(const FieldT &field_coeff, const variable<FieldT> &var)
{
    return linear_combination<FieldT>(field_coeff) - var;
}

template<typename FieldT>
linear_term<FieldT>::linear_term(const variable<FieldT> &var) :
    index(var.index), coeff(FieldT::one())
{
}

template<typename FieldT>
linear_term<FieldT>::linear_term(const variable<FieldT> &var, const integer_coeff_t int_coeff) :
    index(var.index), coeff(FieldT(int_coeff))
{
}

template<typename FieldT>
linear_term<FieldT>::linear_term(const variable<FieldT> &var, const FieldT &coeff) :
    index(var.index), coeff(coeff)
{
}

template<typename FieldT>
linear_term<FieldT> linear_term<FieldT>::operator*(const integer_coeff_t int_coeff) const
{
    return (this->operator*(FieldT(int_coeff)));
}

template<typename FieldT>
linear_term<FieldT> linear_term<FieldT>::operator*(const FieldT &field_coeff) const
{
    return linear_term<FieldT>(this->index, field_coeff * this->coeff);
}

template<typename FieldT>
linear_combination<FieldT> operator+(const integer_coeff_t int_coeff, const linear_term<FieldT> &lt)
{
    return linear_combination<FieldT>(int_coeff) + lt;
}

template<typename FieldT>
linear_combination<FieldT> operator+(const FieldT &field_coeff, const linear_term<FieldT> &lt)
{
    return linear_combination<FieldT>(field_coeff) + lt;
}

template<typename FieldT>
linear_combination<FieldT> operator-(const integer_coeff_t int_coeff, const linear_term<FieldT> &lt)
{
    return linear_combination<FieldT>(int_coeff) - lt;
}

template<typename FieldT>
linear_combination<FieldT> operator-(const FieldT &field_coeff, const linear_term<FieldT> &lt)
{
    return linear_combination<FieldT>(field_coeff) - lt;
}

template<typename FieldT>
linear_combination<FieldT> linear_term<FieldT>::operator+(const linear_combination<FieldT> &other) const
{
    return linear_combination<FieldT>(*this) + other;
}

template<typename FieldT>
linear_combination<FieldT> linear_term<FieldT>::operator-(const linear_combination<FieldT> &other) const
{
    return (*this) + (-other);
}

template<typename FieldT>
linear_term<FieldT> linear_term<FieldT>::operator-() const
{
    return linear_term<FieldT>(this->index, -this->coeff);
}

template<typename FieldT>
bool linear_term<FieldT>::operator==(const linear_term<FieldT> &other) const
{
    return (this->index == other.index &&
            this->coeff == other.coeff);
}

template<typename FieldT>
linear_term<FieldT> operator*(const integer_coeff_t int_coeff, const linear_term<FieldT> &lt)
{
    return FieldT(int_coeff) * lt;
}

template<typename FieldT>
linear_term<FieldT> operator*(const FieldT &field_coeff, const linear_term<FieldT> &lt)
{
    return linear_term<FieldT>(lt.index, field_coeff * lt.coeff);
}

template<typename FieldT>
linear_combination<FieldT>::linear_combination(const integer_coeff_t int_coeff)
{
    this->add_term(linear_term<FieldT>(0, int_coeff));
}

template<typename FieldT>
linear_combination<FieldT>::linear_combination(const FieldT &field_coeff)
{
    this->add_term(linear_term<FieldT>(0, field_coeff));
}

te