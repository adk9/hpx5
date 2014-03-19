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
#ifndef LIBHPX_DEBUG_H
#define LIBHPX_DEBUG_H

#include "hpx.h"

#ifdef ENABLE_DEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif

/// Some output wrappers
HPX_INTERNAL void dbg_log1(const char *f, const char *fmt, ...) HPX_PRINTF(2, 3);
HPX_INTERNAL int dbg_error1(const char *f, const char *fmt, ...) HPX_PRINTF(2, 3);

#ifdef ENABLE_DEBUG
#define dbg_log(...) dbg_log1(__func__, __VA_ARGS__)
#define dbg_error(...) dbg_error1(__func__, __VA_ARGS__)
#else
#define dbg_log(...)
#define dbg_error(...) dbg_error1(__func__, __VA_ARGS__)
#endif

#endif // LIBHPX_DEBUG_H
