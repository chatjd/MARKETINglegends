/** @file
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef PUBLIC_PARAMS_HPP_
#define PUBLIC_PARAMS_HPP_
#include <vector>

namespace libsnark {

/*
  for every curve the user should define corresponding
  public_params with the following typedefs:

  Fp_type
  G1_type
  G2_type
  G1_precomp_type
  G2_precomp_type
  affine_ate_G1_precomp_type
  affine_ate_G2_precomp_type
  Fq_type
  Fqe_type
  Fqk_type
  GT_type

  one should also define the following static methods:

  void init_public_params();

  GT<EC_ppT> final_exponentiation(const Fqk<EC_ppT> &elt);

  G1_precomp<EC_ppT> precompute_G1(const G1<EC_ppT> &P);
  G2_precomp<EC_ppT> precompute_G2(const G2<EC_ppT> &Q);

  Fqk<EC_ppT> miller_loop(const G1_precomp<EC_ppT> &prec_P,
                          const G2_precomp<EC_ppT> &prec_Q);

  affine_ate_G1_precomp<EC_ppT> affine_ate_precompute_G1(const G1<EC_ppT> &P);
  affine_ate_G2_precomp<EC_ppT> affine_ate_precompute_G2(const G2<EC_ppT> &Q);


  Fqk<EC_ppT> affine_ate_miller_loop(const affine_ate_G1_precomp<EC_ppT> &prec_P,
                                     const affine_ate_G2_precomp<EC_ppT> &prec_Q);
  Fqk<EC_ppT> affine_ate_e_over_e_miller_loop(const affine_ate_G1_precomp<EC_ppT> &prec_P1,
                                              const affine_ate_G2_precomp<EC_ppT> &prec_Q1,
                                              const affine_ate_G1_precomp<EC_ppT> &prec_P2,
                                              const affine_ate_G2_precomp<EC_ppT> &prec_Q2);
  Fqk<EC_ppT> affine_ate_e_times_e_over_e_miller_loop(const affine_ate_G1_precomp<EC_ppT> &prec_P1,
                                                      const affine_ate_G2_precomp<EC_ppT> &prec_Q1,
                                                      const affine_ate_G1_precomp<EC_ppT> &prec_P2,
                                                      const affine_ate_G2_precomp<EC_ppT> &prec_Q2,
                                                      const affine_ate_G1_precomp<EC_ppT> &prec_P3,
                                                      const affine_ate_G2_precomp<EC_ppT> &prec_Q3);
  Fqk<EC_ppT> do