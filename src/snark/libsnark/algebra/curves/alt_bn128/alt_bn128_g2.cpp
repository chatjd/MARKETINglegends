/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "algebra/curves/alt_bn128/alt_bn128_g2.hpp"

namespace libsnark {

#ifdef PROFILE_OP_COUNTS
int64_t alt_bn128_G2::add_cnt = 0;
int64_t alt_bn128_G2::dbl_cnt = 0;
#endif

std::vector<size_t> alt_bn128_G2::wnaf_window_table;
std::vector<size_t> alt_bn128_G2::fixed_base_exp_window_table;
alt_bn128_G2 alt_bn128_G2::G2_zero;
alt_bn128_G2 alt_bn128_G2::G2_one;

alt_bn128_G2::alt_bn128_G2()
{
    this->X = G2_zero.X;
    this->Y = G2_zero.Y;
    this->Z = G2_zero.Z;
}

alt_bn128_Fq2 alt_bn128_G2::mul_by_b(const alt_bn128_Fq2 &elt)
{
    return alt_bn128_Fq2(alt_bn128_twist_mul_by_b_c0 * elt.c0, alt_bn128_twist_mul_by_b_c1 * elt.c1);
}

void alt_bn128_G2::print() const
{
    if (this->is_zero())
    {
        printf("O\n");
    }
    else
    {
        alt_bn128_G2 copy(*this);
        copy.to_affine_coordinates();
        gmp_printf("(%Nd*z + %Nd , %Nd*z + %Nd)\n",
                   copy.X.c1.as_bigint().data, alt_bn128_Fq::num_limbs,
                   copy.X.c0.as_bigint().data, alt_bn128_Fq::num_limbs,
                   copy.Y.c1.as_bigint().data, alt_bn128_Fq::num_limbs,
                   copy.Y.c0.as_bigint().data, alt_bn128_Fq::num_limbs);
    }
}

void alt_bn128_G2::print_coordinates() const
{
    if (this->is_zero())
    {
        printf("O\n");
    }
    else
    {
        gmp_printf("(%Nd*z + %Nd : %Nd*z + %Nd