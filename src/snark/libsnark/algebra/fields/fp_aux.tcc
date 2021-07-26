/** @file
 *****************************************************************************
 Assembly code snippets for F[p] finite field arithmetic, used by fp.tcc .
 Specific to x86-64, and used only if USE_ASM is defined.
 On other architectures or without USE_ASM, fp.tcc uses a portable
 C++ implementation instead.
 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef FP_AUX_TCC_
#define FP_AUX_TCC_

namespace libsnark {

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/* addq is faster than adcq, even if preceded by clc */
#define ADD_FIRSTADD                            \
    "movq    (%[B]), %%rax           \n\t"      \
    "addq    %%rax, (%[A])           \n\t"

#define ADD_NEXTADD(ofs)                                \
    "movq    " STR(ofs) "(%[B]), %%rax          \n\t"   \
    "adcq    %%rax, " STR(ofs) "(%[A])          \n\t"

#define ADD_CMP(ofs)                                  \
    "movq    " STR(ofs) "(%[mod]), %%rax   \n\t"      \
    "cmpq    %%rax, " STR(ofs) "(%[A])     \n\t"      \
    "jb      done%=              \n\t"                \
    "ja      subtract%=          \n\t"

#define ADD_FIRSTSUB                            \
    "movq    (%[mod]), %%rax     \n\t"          \
    "subq    %%rax, (%[A])       \n\t"

#define ADD_FIRSTSUB                            \
    "movq    (%[mod]), %%rax     \n\t"          \
    "subq    %%rax, (%[A])       \n\t"

#define ADD_NEXTSUB(ofs)                                \
    "movq    " STR(ofs) "(%[mod]), %%rax    \n\t"       \
    "sbbq    %%rax, " STR(ofs) "(%[A])      \n\t"

#define SUB_FIRSTSUB                            \
    "movq    (%[B]), %%rax\n\t"                 \
    "subq    %%rax, (%[A])\n\t"

#define SUB_NEXTSUB(ofs)                        \
    "movq    " STR(ofs) "(%[B]), %%rax\n\t"     \
    "sbbq    %%rax, " STR(ofs) "(%[A])\n\t"

#define SUB_FIRSTADD                            \
    "movq    (%[mod]), %%rax\n\t"               \
    "addq    %%rax, (%[A])\n\t"

#define SUB_NEXTADD(ofs)                        \
    "movq    " STR(ofs) "(%[mod]), %%rax\n\t"   \
    "adcq    %%rax, " STR(ofs) "(%[A])\n\t"

#define MONT_CMP(ofs)                                 \
    "movq    " STR(ofs) "(%[M]), %%rax   \n\t"        \
    "cmpq    %%rax, " STR(ofs) "(%[tmp])   \n\t"      \
    "jb      done%=              \n\t"                \
    "ja      subtract%=          \n\t"

#define MONT_FIRSTSUB                           \
    "movq    (%[M]), %%rax     \n\t"            \
    "subq    %%rax, (%[tmp])     \n\t"

#define MONT_NEXTSUB(ofs)                               \
    "movq    " STR(ofs) "(%[M]), %%rax    \n\t"         \
    "sbbq    %%rax, " STR(ofs) "(%[tmp])   \n\t"

/*
  The x86-64 Montgomery multiplication here is similar
  to Algorithm 2 (CIOS method) in http://eprint.iacr.org/2012/140.pdf
  and the PowerPC pseudocode of gmp-ecm library (c) Paul Zimmermann and Alexander Kruppa
  (see comments on top of powerpc64/mulredc.m4).
*/

#define MONT_PRECOMPUTE                                                 \
    "xorq    %[cy], %[cy]            \n\t"                              \
    "movq    0(%[A]), %%rax          \n\t"                              \
    "mulq    0(%[B])                 \n\t"                              \
    "movq    %%rax, %[T0]            \n\t"                              \
    "movq    %%rdx, %[T1]            # T1:T0 <- A[0] * B[0] \n\t"       \
    "mulq    %[inv]                  \n\t"                              \
    "movq    %%rax, %[u]             # u <- T0 * inv \n\t"              \
    "mulq    0(%[M])                 \n\t"                              \
    "addq    %[T0], %%rax            \n\t"                              \
    "adcq    %%rdx, %[T1]            \n\t"                              \
    "adcq    $0, %[cy]               # cy:T1 <- (M[0]*u + T1 * b + T0) / b\n\t"

#define MONT_FIRSTITER(j)                                               \
    "xorq    %[T0], %[T0]            \n\t"                              \
    "movq    0(%[A]), %%rax          \n\t"                              \
    "mulq    " STR((j*8)) "(%[B])                 \n\t"                 \
    "addq    %[T1], %%rax            \n\t"                              \
    "movq    %%rax, " STR(((j-1)*8)) "(%[tmp])        \n\t"             \
    "adcq    $0, %%rdx               \n\t"                              \
    "movq    %%rdx, %[T1]            # now T1:tmp[j-1] <-- X[0] * Y[j] + T1\n\t" \
    "movq    " STR((j*8)) "(%[M]), %%rax          \n\t"                 \
    "mulq    %[u]                    \n\t"                              \
    "addq    %%rax, " STR(((j-1)*8)) "(%[tmp])        \n\t"             \
    "adcq    %[cy], %%rdx            \n\t"                              \
    "adcq    $0, %[T0]               \n\t"                              \
    "xorq    %[cy], %[cy]            \n\t"                              \
    "addq    %%rdx, %[T1]            \n\t"                              \
    "adcq    %[T0], %[cy]            # cy:T1:tmp[j-1] <---- (X[0] * Y[j] + T1) + (M[j] * u + cy * b) \n\t"

#define MONT_ITERFIRST(i)                            \
    "xorq    %[cy], %[cy]            \n\t"              \
    "movq    " STR((i*8)) "(%[A]), %%rax          \n\t" \
    "mulq    0(%[B])                 \n\t"              \
    "addq    0(%[tmp]), %%rax        \n\t"              \
    "adcq    8(%[tmp]), %%rdx        \n\t"              \
    "adcq    $0, %[cy]               \n\t"              \
    "movq    %%rax, %[T0]            \n\t"                              \
    "movq    %%rdx, %[T1]            # cy:T1:T0 <- A[i] * B[0] + tmp[1] * b + tmp[0]\n\t" \
    "mulq    %[inv]                  \n\t"                              \
    "movq    %%rax, %[u]             # u <- T0 * inv\n\t"               \
    "mulq    0(%[M])                 \n\t"                              \
    "addq    %[T0], %%rax            \n\t"                              \
    "adcq    %%rdx, %[T1]            \n\t"                              \
    "adcq    $0, %[cy]               # cy:T1 <- (M[0]*u + cy * b * b + T1 * b + T0) / b\n\t"

#define MONT_ITERITER(i, j)                                             \
    "xorq    %[T0], %[T0]            \n\t"                              \
    "movq    " STR((i*8)) "(%[A]), %%rax          \n\t"                 \
    "mulq    " STR((j*8)) "(%[B])                 \n\t"                 \
    "addq    %[T1], %%rax            \n\t"                              \
    "movq    %%rax, " STR(((j-1)*8)) "(%[tmp])        \n\t"                              \
    "adcq    $0, %%rdx               \n\t"                              \
    "movq    %%rdx, %[T1]            # now T1:tmp[j-1] <-- X[i] * Y[j] + T1 \n\t" \
    "movq    " STR((j*8)) "(%[M]), %%rax          \n\t"                 \
    "mulq    %[u]                    \n\t"                              \
    "addq    %%rax, " STR(((j-1)*8)) "(%[tmp])        \n\t"             \
    "adcq    %[cy], %%rdx            \n\t"                              \
    "adcq    $0, %[T0]               \n\t"                              \
    "xorq    %[cy], %[cy]            \n\t"                              \
    "addq    %%rdx, %[T1]            \n\t"                              \
    "adcq    %[T0], %[cy]            # cy:T1:tmp[j-1] <-- (X[i] * Y[j] + T1) + M[j] * u + cy * b \n\t" \
    "addq    " STR(((j+1)*8)) "(%[tmp]), %[T1]       \n\t" \
    "adcq    $0, %[cy]               # cy:T1:tmp[j-1] <-- (X[i] * Y[j] + T1) + M[j] * u + (tmp[j+1] + cy) * b \n\t"

#define MONT_FINALIZE(j)                  \
    "movq    %[T1], " STR((j*8)) "(%[tmp])       \n\t"          \
    "movq    %[cy], " STR(((j+1)*8)) "(%[tmp])       \n\t"

/*
  Comba multiplication and squaring routines are based on the
  public-domain tomsfastmath library by Tom St Denis
  <http://www.libtom.org/>
  <https://github.com/libtom/tomsfastmath/blob/master/src/sqr/fp_sqr_comba.c
  <https://github.com/libtom/tomsfastmath/blob/master/src/mul/fp_mul_comba.c>

  Compared to the above, we save 5-20% of cycles by using careful register
  renaming to implement Comba forward operation.
 */

#define COMBA_3_BY_3_MUL(c0_, c1_, c2_, res_, A_, B_)    \
    asm volatile (                              \
        "movq  0(%[A]), %%rax             \n\t" \
        "mulq  0(%[B])                    \n\t" \
        "movq  %%rax, 0(%[res])           \n\t" \
        "movq  %%rdx, %[c0]               \n\t" \
                                                \
        "xorq  %[c1], %[c1]               \n\t" \
        "movq  0(%[A]), %%rax             \n\t" \
        "mulq  8(%[B])                    \n\t" \
        "addq  %%rax, %[c0]               \n\t" \
        "adcq  %%rdx, %[c1]               \n\t" \
                                                \
        "xorq  %[c2], %[c2]               \n\t" \
        "movq  8(%[A]), %%rax             \n\t" \
        "mulq  0(%[B])                    \n\t" \
        "addq  %%rax, %[c0]               \n\t" \
        "movq  %[c0], 8(%[res])           \n\t" \
        "adcq  %%rdx, %[c1]               \n\t" \
        "adcq  $0, %[c2]                  \n\t" \
                                                \
        "// register renaming (c1, c2, c0)\n\t" \
        "xorq  %[c0], %[c0]               \n\t" \
        "movq  0(%[A]), %%rax             \n\t" \
        "mulq  16(%[B])                   \n\t" \
        "addq  %%rax, %[c1]               \n\t" \
        "adcq  %%rdx, %[c2]               \n\t" \
        "adcq  $0, %[c0]                  \n\t" \
                                                \
        "movq  8(%[A]), %%rax             \n\t" \
        "mulq  8(%[B])                    \n\t" \
        "addq  %%rax, %[c1]               \n\t" \
        "adcq  %%rdx, %[c2]               \n\t" \
        "adcq  $0, %[c0]                  \n\t" \
                                                \
       