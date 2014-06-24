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
#ifndef HPX_ATTRIBUTES_H
#define HPX_ATTRIBUTES_H

/// @file
/// @brief Attribute definitions for HPX functions.

#define HPX_ALIGNED(N)       __attribute__((aligned(N)))
#define HPX_INTERNAL         __attribute__((visibility("internal")))
#define HPX_RETURNS_TWICE    __attribute__((returns_twice))
#define HPX_NORETURN         __attribute__((noreturn))
#define HPX_NOINLINE         __attribute__((noinline))
#define HPX_AWAYS_INLINE     __attribute__((always_inline))
#define HPX_OPTIMIZE(S)      __attribute__((optimize(S)))
#define HPX_MALLOC           __attribute__((malloc))
#define HPX_USED             __attribute__((used))
#define HPX_UNUSED           __attribute__((unused))
#define HPX_ASM(S)           __asm__(#S)
#define HPX_PACKED           __attribute__((packed))
#define HPX_NON_NULL(...)    __attribute__((nonnull(__VA_ARGS__)))
#define HPX_WEAK             __attribute__((weak))
#define HPX_CONSTRUCTOR      __attribute__((constructor))
#define HPX_DESTRUCTOR       __attribute__((destructor))
#define HPX_PRINTF(f, s)     __attribute__((format (printf, f, s)))
#define HPX_RETURNS_NON_NULL /*__attribute__((returns_nonnull))*/

#endif // HPX_ATTRIBUTES_H
