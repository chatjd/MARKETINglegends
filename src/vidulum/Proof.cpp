#include "Proof.hpp"

#include "crypto/common.h"

#include <boost/static_assert.hpp>
#include <libsnark/common/default_types/r1cs_ppzksnark_pp.hpp>
#include <libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp>
#include <mutex>

using namespace libsnark;

typedef alt_bn128_pp curve_pp;
typedef alt_bn128_pp::G1_type curve_G1;
typedef alt_bn128_pp::G2_type curve_G2;
typedef alt_bn128_pp::GT_type curve_GT;
typedef alt_bn128_pp::Fp_type curve_Fr;
typedef alt_bn