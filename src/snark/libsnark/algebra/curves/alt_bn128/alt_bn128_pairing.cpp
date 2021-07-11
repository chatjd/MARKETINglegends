/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "algebra/curves/alt_bn128/alt_bn128_pairing.hpp"
#include "algebra/curves/alt_bn128/alt_bn128_init.hpp"
#include "algebra/curves/alt_bn128/alt_bn128_g1.hpp"
#include "algebra/curves/alt_bn128/alt_bn128_g2.hpp"
#include <cassert>
#include "common/profiling.hpp"
#include "common/assert_except.hpp"

namespace libsnark {

bool alt_bn128_ate_G1_precomp::operator==(const alt_bn128_ate_G1_precomp &other) const
{
    return (this->PX == other.PX &&
            this->PY == other.PY);
}

std::ostream& operator<<(std::ostream &out, const alt_bn128_ate_G1_precomp &prec_P)
{
    out << prec_P.PX << OUTPUT_SEPARATOR << prec_P.PY;

    return out;
}

std::istream& operator>>(std::istream &in, alt_bn128_ate_G1_precomp &prec_P)
{
    in >> prec_P.PX;
    consume_OUTPUT_SEPARATOR(in);
    in >> prec_P.PY;

    return in;
}

bool  alt_bn128_ate_ell_coeffs::operator==(const alt_bn128_ate_ell_coeffs &other) const
{
    return (this->ell_0 == other.ell_0 &&
            this->ell_VW == other.ell_VW &&
            this->ell_VV == other.ell_VV);
}

std::ostream& operator<<(std::ostream &out, const alt_bn128_ate_ell_coeffs &c)
{
    out << c.ell_0 << OUTPUT_SEPARATOR << c.ell_VW << OUTPUT_SEPARATOR << c.ell_VV;
    return out;
}

std::istream& operator>>(std::istream &in, alt_bn128_ate_ell_coeffs &c)
{
    in >> c.ell_0;
    consume_OUTPUT_SEPARATOR(in);
    in >> c.ell_VW;
    consume_OUTPUT_SEPARATOR(in);
    in >> c.ell_VV;

    return in;
}

bool alt_bn128_ate_G2_precomp::operator==(const alt_bn128_ate_G2_precomp &other) const
{
    return (this->QX == other.QX &&
            this->QY == other.QY &&
            this->coeffs == other.coeffs);
}

std::ostream& operator<<(std::ostream& out, const alt_bn128_ate_G2_precomp &prec_Q)
{
    out << prec_Q.QX << OUTPUT_SEPARATOR << prec_Q.QY << "\n";
    out << prec_Q.coeffs.size() << "\n";
    for (const alt_bn128_ate_ell_coeffs &c : prec_Q.coeffs)
    {
        out << c << OUTPUT_NEWLINE;
    }
    return out;
}

std::istream& operator>>(std::istream& in, alt_bn128_ate_G2_precomp &prec_Q)
{
    in >> prec_Q.QX;
    consume_OUTPUT_SEPARATOR(in);
    in >> prec_Q.QY;
    consume_newline(in);

    prec_Q.coeffs.clear();
    size_t s;
    in >> s;

    consume_newline(in);

    prec_Q.coeffs.reserve(s);

    for (size_t i = 0; i < s; ++i)
    {
        alt_bn128_ate_ell_coeffs c;
        in >> c;
        consume_OUTPUT_NEWLINE(in);
        prec_Q.coeffs.emplace_back(c);
    }

    return in;
}

/* final exponentiations */

alt_bn128_Fq12 alt_bn128_final_exponentiation_first_chunk(const alt_bn128_Fq12 &elt)
{
    enter_block("Call to alt_bn128_final_exponentiation_first_chunk");

    /*
      Computes result = elt^((q^6-1)*(q^2+1)).
      Follows, e.g., Beuchat et al page 9, by computing result as follows:
         elt^((q^6-1)*(q^2+1)) = (conj(elt) * elt^(-1))^(q^2+1)
      More precisely:
      A = conj(elt)
      B = elt.inverse()
      C = A * B
      D = C.Frobenius_map(2)
      result = D * C
    */

    const alt_bn128_Fq12 A = alt_bn128_Fq12(elt.c0,-elt.c1);
    const alt_bn128_Fq12 B = elt.inverse();
    const alt_bn128_Fq12 C = A * B;
    const alt_bn128_Fq12 D = C.Frobenius_map(2);
    const alt_bn128_Fq12 result = D * C;

    leave_block("Call to alt_bn128_final_exponentiation_first_chunk");

    return result;
}

alt_bn128_Fq12 alt_bn128_exp_by_neg_z(const alt_bn128_Fq12 &elt)
{
    enter_block("Call to alt_bn128_exp_by_neg_z");

    alt_bn128_Fq12 result = elt.cyclotomic_exp(alt_bn128_final_exponent_z);
    if (!alt_bn128_final_exponent_is_z_neg)
    {
        result = result.unitary_inverse();
    }

    leave_block("Call to alt_bn128_exp_by_neg_z");

    return result;
}

alt_bn128_Fq12 alt_bn128_final_exponentiation_last_chunk(const alt_bn128_Fq12 &elt)
{
    enter_block("Call to alt_bn128_final_exponentiation_last_chunk");

    /*
      Follows Laura Fuentes-Castaneda et al. "Faster hashing to G2"
      by computing:

      result = elt^(q^3 * (12*z^3 + 6z^2 + 4z - 1) +
                    q^2 * (12*z^3 + 6z^2 + 6z) +
                    q   * (12*z^3 + 6z^2 + 4z) +
                    1   * (12*z^3 + 12z^2 + 6z + 1))
      which equals

      result = elt^( 2z * ( 6z^2 + 3z + 1 ) * (q^4 - q^2 + 1)/r ).

      Using the following addition chain:

      A = exp_by_neg_z(elt)  // = elt^(-z)
      B = A^2                // = elt^(-2*z)
      C = B^2                // = elt^(-4*z)
      D = C * B              // = elt^(-6*z)
      E = exp_by_neg_z(D)    // = elt^(6*z^2)
      F = E^2                // = elt^(12*z^2)
      G = epx_by_neg_z(F)    // = elt^(-12*z^3)
      H = conj(D)            // = elt^(6*z)
      I = conj(G)            // = elt^(12*z^3)
      J = I * E              // = elt^(12*z^3 + 6*z^2)
      K = J * H              // = elt^(12*z^3 + 6*z^2 + 6*z)
      L = K * B              // = elt^(12*z^3 + 6*z^2 + 4*z)
      M = K * E              // = elt^(12*z^3 + 12*z^2 + 6*z)
      N = M * elt            // = elt^(12*z^3 + 12*z^2 + 6*z + 1)
      O = L.Frobenius_map(1) // = elt^(q*(12*z^3 + 6*z^2 + 4*z))
      P = O * N              // = elt^(q*(12*z^3 + 6*z^2 + 4*z) * (12*z^3 + 12*z^2 + 6*z + 1))
      Q = K.Frobenius_map(2) // = elt^(q^2 * (12*z^3 + 6*z^2 + 6*z))
      R = Q * P              // = elt^(q^2 * (12*z^3 + 6*z^2 + 6*z) + q*(12*z^3 + 6*z^2 + 4*z) * (12*z^3 + 12*z^2 + 6*z + 1))
      S = conj(elt)          // = elt^(-1)
      T = S * L              // = elt^(12*z^3 + 6*z^2 + 4*z - 1)
      U = T.Frobenius_map(3) // = elt^(q^3(12*z^3 + 6*z^2 + 4*z - 1))
      V = U * R              // = elt^(q^3(12*z^3 + 6*z^2 + 4*z - 1) + q^2 * (12*z^3 + 6*z^2 + 6*z) + q*(12*z^3 + 6*z^2 + 4*z) * (12*z^3 + 12*z^2 + 6*z + 1))
      result = V

    */

    const alt_bn128_Fq12 A = alt_bn128_exp_by_neg_z(elt);
    const alt_bn128_Fq12 B = A.cyclotomic_squared();
    const alt_bn128_Fq12 C = B.cyclotomic_squared();
    const alt_bn128_Fq12 D = C * B;
    const