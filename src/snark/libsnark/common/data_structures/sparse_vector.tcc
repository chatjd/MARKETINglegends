/** @file
 *****************************************************************************

 Implementation of interfaces for a sparse vector.

 See sparse_vector.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef SPARSE_VECTOR_TCC_
#define SPARSE_VECTOR_TCC_

#include "algebra/scalar_multiplication/multiexp.hpp"

#include <numeric>

namespace libsnark {

template<typename T>
sparse_vector<T>::sparse_vector(std::vector<T> &&v) :
    values(std::move(v)), domain_size_(values.size())
{
    indices.resize(domain_size_);
    std::iota(indices.begin(), indices.end(), 0);
}

template<typename T>
T sparse_vector<T>::operator[](const size_t idx) const
{
    auto it = std::lower_bound(indices.begin(), indices.end(), idx);
    return (it != indices.end() && *it == idx) ? values[it - indices.begin()] : T();
}

template<typename T>
bool sparse_vector<T>::operator==(const sparse_vector<T> &other) const
{
    if (this->domain_size_ != other.domain_size_)
    {
        return false;
    }

    size_t this_pos = 0, other_pos = 0;
    while (this_pos < this->indices.size() && other_pos < other.indices.size())
    {
        if (this->indices[this_pos] == other.indices[other_pos])
        {
            if (this->values[this_pos] != other.values[other_pos])
            {
                return false;
            }
            ++this_pos;
            ++other_pos;
        }
        else if (this->indices[this_pos] < other.indices[other_pos])
        {
            if (!this->values[this_pos].is_zero())
            {
                return false;
            }
            ++this_pos;
        }
        else
        {
            if (!other.values[other_pos].is_zero())
            {
                return false;
            }
            ++other_pos;
        }
    }

    /* at least one of the vectors has been exhausted, so other must be empty */
    while (this_pos < this->indices.size())
    {
        if (!this->values[this_pos].is_zero())
        {
            return false;
        }
        ++this_pos;
    }

    while (other_pos < other.indices.size())
    {
        if (!other.values[other_pos].is_zero())
        {
            return false;
        }
        ++other_pos;
    }

    return true;
}

template<typename T>
bool sparse_vector<T>::operator==(const std::vector<T> &other) const
{
    if (this->domain_size_ < other.size())
    {
        return false;
    }

    size_t j = 0;
    for (size_t i = 0; i < other.size(); ++i)
    {
        if (this->indices[j] == i)
        {
            if (this->values[j] != other[j])
            