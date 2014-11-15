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

#include "hpx/hpx.h"
#include "libhpx/config.h"

HPX_INTERNAL extern hpx_log_t dbg_log_level;

#ifdef ENABLE_DEBUG
#define DEBUG 1
#define DEBUG_IF(S) if (S)
#else
#define DEBUG 0
#define DEBUG_IF(S) if (false && S)
#endif

/// Some output wrappers
HPX_INTERNAL void dbg_log1(unsigned line, const char *f, const hpx_log_t level, const char *fmt, ...) HPX_PRINTF(4, 5);
HPX_INTERNAL int dbg_error1(unsigned line, const char *f, const char *fmt, ...) HPX_PRINTF(3, 4);

#ifdef ENABLE_DEBUG
#define _dbg_log(...) dbg_log1(__LINE__, __func__, __VA_ARGS__)
#define dbg_error(...) dbg_error1(__LINE__, __func__, __VA_ARGS__)
#define dbg_check(e, ...) do { if (e != HPX_SUCCESS) dbg_error(__VA_ARGS__); } while (0)
#define dbg_assert(e) assert(e)

#elif defined(NDEBUG)
#define _dbg_log(...)
#define dbg_error(...) dbg_error1(__LINE__, __func__, __VA_ARGS__)
#define dbg_check(e, ...) (void)e
#define dbg_assert(e)
#else
#define _dbg_log(...)
#define dbg_error(...) dbg_error1(__LINE__, __func__, __VA_ARGS__)
#define dbg_check(e, ...) assert(e == HPX_SUCCESS)
#define dbg_assert(e)
#endif

HPX_INTERNAL void dbg_wait(void);

/// Wrappers to log level-specific debug messages
#define dbg_log(...) _dbg_log(HPX_LOG_DEFAULT, __VA_ARGS__)
#define dbg_log_boot(...) _dbg_log(HPX_LOG_BOOT, __VA_ARGS__)
#define dbg_log_sched(...) _dbg_log(HPX_LOG_SCHED, __VA_ARGS__)
#define dbg_log_lco(...) _dbg_log(HPX_LOG_LCO, __VA_ARGS__)
#define dbg_log_gas(...) _dbg_log(HPX_LOG_GAS, __VA_ARGS__)
#define dbg_log_net(...) _dbg_log(HPX_LOG_NET, __VA_ARGS__)
#define dbg_log_trans(...) _dbg_log(HPX_LOG_TRANS, __VA_ARGS__)
#define dbg_log_parcel(...) _dbg_log(HPX_LOG_PARCEL, __VA_ARGS__)

#endif // LIBHPX_DEBUG_H
