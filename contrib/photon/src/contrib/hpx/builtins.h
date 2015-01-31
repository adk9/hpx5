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
  _Pragma("GCC diagnostic push")                  \
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

static inline uint32_t ceil_div_32(uint32_t num, uint32_t denom) {
  return (num / denom) + ((num % denom) ? 1 : 0);
}

static inline uint64_t ceil_div_64(uint64_t num, uint64_t denom) {
  return (num / denom) + ((num % denom) ? 1 : 0);
}

#endif // HPX_BUILTINS_H
