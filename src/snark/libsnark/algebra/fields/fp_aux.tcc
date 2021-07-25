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
    "adcq    %%rdx, %[T1]            \n\t"                  