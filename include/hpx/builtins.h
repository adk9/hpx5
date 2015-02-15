// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef HPX_BUILTINS_H
#define HPX_BUILTINS_H

// ----------------------------------------------------------------------------
/// @file builtins.h
///
/// This file encapsulates some operations that are builtin compiler
/// functions in order to support multiple compilers.
///
/// @todo Deal with non-gcc compatible compilers.
/// @todo Deal with this during configuration.
// ----------------------------------------------------------------------------

#define likely(S) (__builtin_expect(S, 1))
#define unlikely(S) (__builtin_expect(S, 0))
#if (__GNUC_MINOR__ > 5) || defined(__INTEL_COMPILER) || defined(__clang__)
#define unreachable() __builtin_unreachable()
#else
#define unreachable()
#endif
#define ctzl(N) __builtin_ctzl(N)
#define clzl(N) __builtin_clzl(N)
#define clz(N) __builtin_clz(N)

#define popcountl(N) __builtin_popcountl(N)

#if (__GNUC__ > 3) && (__GNUC_MINOR__ > 5)
#define _HELPER0(x) #x
#define _HELPER1(x) _HELPER0(GCC diagnostic ignored x)
#define _HELPER2(y) _HELPER1(#y)
#define HPX_PUSH_IGNORE(S)                      \
  _Pragma("GCC diagnostic push")                \
  _Pragma(_HELPER2(S))
#define HPX_POP_IGNORE                          \
  _Pragma("GCC diagnostic pop")
#else
#define HPX_PUSH_IGNORE(S)
#define HPX_POP_IGNORE
#endif

#include <stdint.h>

/// From stack overflow.
///
/// http://stackoverflow.com/questions/3272424/compute-fast-log-base-2-ceiling
static inline uint32_t ceil_log2_32(uint32_t val) {
  return ((sizeof(val) * 8 - 1) - clz(val)) + (!!(val & (val - 1)));
}

static inline uint32_t ceil_log2_64(uint64_t val) {
  return ((sizeof(val) * 8 - 1) - clzl(val)) + (!!(val & (val - 1)));
}

static inline uint32_t ceil_div_32(uint32_t num, uint32_t denom) {
  return (num / denom) + ((num % denom) ? 1 : 0);
}

static inline uint64_t ceil_div_64(uint64_t num, uint64_t denom) {
  return (num / denom) + ((num % denom) ? 1 : 0);
}

static inline int min_int(int lhs, int rhs) {
  return (lhs < rhs) ? lhs : rhs;
}

static inline uint64_t min_u64(uint64_t lhs, uint64_t rhs) {
  return (lhs < rhs) ? lhs : rhs;
}

static inline int32_t max_i32(int32_t lhs, int32_t rhs) {
  return (lhs < rhs) ? rhs : lhs;
}

/// Miscellaneous utility macros.

#define _HPX_XSTR(s) _HPX_STR(s)
#define _HPX_STR(l) #l

/// Macro to count the number of variadic arguments
/// Source: https://groups.google.com/forum/#!topic/comp.std.c/d-6Mj5Lko_s
#define __HPX_NARGS(...) __VA_NARG__(__VA_ARGS__)
#define __VA_NARG__(...) (__VA_NARG_(_0, ## __VA_ARGS__, __RSEQ_N()) - 1)
#define __VA_NARG_(...) __VA_ARG_N(__VA_ARGS__)
#define __VA_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, \
                   _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, \
                   _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, \
                   _39, _40, _41, _42, _43, _44, _45, _46,_47, _48, _49, _50, \
                   _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62, \
                   _63, _,...) _

#define __RSEQ_N() 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, \
    48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, \
    29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, \
    10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0

#endif // HPX_BUILTINS_H
