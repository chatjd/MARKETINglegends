/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "algebra/curves/alt_bn128/alt_bn128_g1.hpp"

namespace libsnark {

#ifdef PROFILE_OP_COUNTS
int64_t alt_bn128_G1::add_cnt = 0;
int64_t alt_bn128_G1::dbl_cnt = 0;
#endif

std::vector<size_t> alt_bn128_G1::wnaf_window_table;
std::vector<size_t> alt_bn128_G1::fixed_base_exp_window_table;
alt_bn128_G1 alt_bn128_G1::G1_zero;
alt_bn128_G1 alt_bn128_G1::G1_one;

alt_bn128_G1::alt_bn128_G1()
{
    this->X = G1_zero.X;
    this->Y = G1_zero.Y;
    this->Z = G1_zero.Z;
}

void alt_bn128_G1::print() const
{
    if (this->is_zero())
    {
        printf("O\n");
    }
    else
    {
        alt_bn128_G1 copy(*this);
        copy.to_affine_coordinates();
        gmp_printf("(%Nd , %Nd)\n",
                   copy.X.as_bigint().data, alt_bn128_Fq::num_limbs,
                   copy.Y.as_bigint().data, alt_bn128_Fq::num_limbs);
    }
}

void alt_bn128_G1::print_coordinates() const
{
    if (this->is_zero())
    {
        printf("O\n");
    }
    else
    {
        gmp_printf("(%Nd : %Nd : %Nd)\n",
                   this->X.as_bigint().data, alt_bn128_Fq::num_limbs,
                   this->Y.as_bigint().data, alt_bn128_Fq::num_limbs,
                   this->Z.as_bigint().data, alt_bn128_Fq::num_limbs);
    }
}

void alt_bn128_G1::to_affine_coordinates()
{
    if (this->is_zero())
    {
        this->X = alt_bn128_Fq::zero();
        this->Y = alt_bn128_Fq::one();
        this->Z = alt_bn128_Fq::zero();
    }
    else
    {
        alt_bn128_Fq Z_inv = Z.inverse();
        alt_bn128_Fq Z2_inv = Z_inv.squared();
        alt_bn128_Fq Z3_inv = Z2_inv * Z_inv;
        this->X = this->X * Z2_inv;
        this->Y = this->Y * Z3_inv;
        this->Z = alt_bn128_Fq::one();
    }
}

void alt_bn128_G1::to_special()
{
    this->to_affine_coordinates();
}

bool alt_bn128_G1::is_special() const
{
    return (this->is_zero() || this->Z == alt_bn128_Fq::one());
}

bool alt_bn128_G1::is_zero() const
{
    return (this->Z.is_zero());
}

bool alt_bn128_G1::operator==(const alt_bn128_G1 &other) const
{
    if (this->is_zero())
    {
        return other.is_zero();
    }

    if (other.is_zero())
    {
        return false;
    }

    /* now neither is O */

    // using Jacobian coordinates so:
    // (X1:Y1:Z1) = (X2:Y2:Z2)
    // iff
    // X1/Z1^2 == X2/Z2^2 and Y1/Z1^3 == Y2/Z2^3
    // iff
    // X1 * Z2^2 == X2 * Z1^2 and Y1 * Z2^3 == Y2 * Z1^3

    alt_bn128_Fq Z1_squared = (this->Z).squared();
    alt_bn128_Fq Z2_squared = (other.Z).squared();

    if ((this->X * Z2_squared) != (other.X * Z1_squared))
    {
        return false;
    }

    alt_bn128_Fq Z1_cubed = (this->Z) * Z1_squared;
    alt_bn128_Fq Z2_cubed = (other.Z) * Z2_squared;

    if ((this->Y * Z2_cubed) != (other.Y * Z1_cubed))
    {
        return false;
    }

    return true;
}

bool alt_bn128_G1::operator!=(const alt_bn128_G1& other) const
{
    return !(operator==(other));
}

alt_bn128_G1 alt_bn128_G1::operator+(const alt_bn128_G1 &other) const
{
    // handle special cases having to do with O
    if (this->is_zero())
    {
        return other;
    }

    if (other.is_zero())
    {
        return *this;
    }

    // no need to handle points of order 2,4
    // (they cannot exist in a prime-order subgroup)

    // check for doubling case

    // using Jacobian coordinates so:
    // (X1:Y1:Z1) = (X2:Y2:Z2)
    // iff
    // X1/Z1^2 == X2/Z2^2 and Y1/Z1^3 == Y2/Z2^3
    // iff
    // X1 * Z2^2 == X2 * Z1^2 and Y1 * Z2^3 == Y2 * Z1^3

    alt_bn128_Fq Z1Z1 = (this->Z).squared();
    alt_bn128_Fq Z2Z2 = (other.Z).squared();

    alt_bn128_Fq U1 = this->X * Z2Z2;
    alt_bn128_Fq U2 = other.X * Z1Z1;

    alt_bn128_Fq Z1_cubed = (this->Z) * Z1Z1;
    alt_bn128_Fq Z2_cubed = (other.Z) * Z2Z2;

    alt_bn128_Fq S1 = (this->Y) * Z2_cubed;      // S1 = Y1 * Z2 * Z2Z2
    alt_bn128_Fq S2 = (other.Y) * Z1_cubed;      // S2 = Y2 * Z1 * Z1Z1

    if (U1 == U2 && S1 == S2)
    {
        // dbl case; nothing of above can be reused
        return this->dbl();
    }

    // rest of add case
    alt_bn128_Fq H = U2 - U1;                            // H = U2-U1
    alt_bn128_Fq S2_minus_S1 = S2-S1;
    alt_bn128_Fq I = (H+H).squared();                    // I = (2 * H)^2
    alt_bn128_Fq J = H * I;                              // J = H * I
    alt_bn128_Fq r = S2_minus_S1 + S2_minus_S1;          // r = 2 * (S2-S1)
    alt_bn128_Fq V = U1 * I;                             // V = U1 * I
    alt_bn128_Fq X3 = r.squared() - J - (V+V);           // X3 = r^2 - J - 2 * V
    alt_bn128_Fq S1_J = S1 * J;
    alt_bn128_Fq Y3 = r * (V-X3) - (S1_J+S1_J);          // Y3 = r * (V-X3)-2 S1 J
    alt_bn128_Fq Z3 = ((this->Z+other.Z).squared()-Z1Z1-Z2Z2) * H; // Z3 = ((Z1+Z2)^2-Z1Z1-Z2Z2) * H

    return alt_bn128_G1(X3, Y3, Z3);
}

alt_bn128_G1 alt_bn128_G1::operator-() const
{
    return alt_bn128_G1(this->X, -(this->Y), this->Z);
}


alt_bn128_G1 alt_bn128_G1::operator-(const alt_bn128_G1 &other) const
{
    return (*this) + (-other);
}

alt_bn128_G1 alt_bn128_G1::add(const alt_bn128_G1 &other) const
{
    // handle special cases having to do with O
    if (this->is_zero())
    {
        return other;
    }

    if (other.is_zero())
    {
        return *this;
    }

    // no need to handle points of order 2,4
    // (they cannot exist in a prime-order subgroup)

    // handle double case
    if (this->operator==(other))
    {
        return this->dbl();
    }

#ifdef PROFILE_OP_COUNTS
    this->add_cnt++;
#endif
    // NOTE: does not handle O and pts of order 2,4
    // http://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-0.html#addition-add-2007-bl

    alt_bn128_Fq Z1Z1 = (this->Z).squared();             // Z1Z1 = Z1^2
    alt_bn128_Fq Z2Z2 = (other.Z).squared();             // Z2Z2 = Z2^2
    alt_bn128_Fq U1 = (this->X) * Z2Z2;                  // U1 = X1 * Z2Z2
    alt_bn128_Fq U2 = (other.X) * Z1Z1;                  // U2 = X2 * Z1Z1
    alt_bn128_Fq S1 = (this->Y) * (other.Z) * Z2Z2;      // S1 = Y1 * Z2 * Z2Z2
    alt_bn128_Fq S2 = (othe