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

/// ----------------------------------------------------------------------------
/// @file builtins.h
///
/// This file encapsulates some operations that are builtin compiler functions
/// in order to support multiple compilers.
///
/// @todo Deal with non-gcc compatible compilers.
/// @todo Deal with this during configuration.
/// ----------------------------------------------------------------------------

#define likely(S) (__builtin_expect(S, 1))
#define unlikely(S) (__builtin_expect(S, 0))
#define unreachable() __builtin_unreachable()
#define ctzl(N) __builtin_ctzl(N)

#endif // HPX_BUILTINS_H
