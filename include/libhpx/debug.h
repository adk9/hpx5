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

HPX_INTERNAL extern hpx_log_t log_level;

#ifdef ENABLE_DEBUG
#define DEBUG 1
#define DEBUG_IF(S) if (S)
#else
#define DEBUG 0
#define DEBUG_IF(S) if (false)
#endif

/// Wait for the debugger.
void dbg_wait(void)
  HPX_INTERNAL;

int dbg_error_internal(unsigned line, const char *filename, const char *fmt,
                       ...)
  HPX_INTERNAL HPX_PRINTF(3, 4);

void dbg_assert_str_internal(bool expression, unsigned line,
                             const char *filename, const char *fmt, ...)
  HPX_INTERNAL HPX_PRINTF(4, 5);

#ifdef ENABLE_DEBUG

// NB: this is complex for clang's benefit, so it can tell that we're asserting
// e when doing static analysis
#define dbg_assert_str(e, ...)                                       \
  do {                                                               \
    typeof(e) _e = (e);                                              \
    dbg_assert_str_internal(_e, __LINE__, __func__, __VA_ARGS__);    \
    assert(_e);                                                      \
  } while (0)


#define dbg_assert(e) dbg_assert_str(e, "assert failed\n")
#else
#define dbg_assert_str(e, ...) assert(e)
#define dbg_assert(e) assert(e);
#endif

#define dbg_error(...) dbg_error_internal(__LINE__, __func__, __VA_ARGS__)
#define dbg_check(e, ...) dbg_assert_str((e) == HPX_SUCCESS, __VA_ARGS__)

void log_internal(const hpx_log_t level, unsigned line, const char *filename,
                  const char *fmt, ...)
  HPX_INTERNAL HPX_PRINTF(4, 5);

#ifdef ENABLE_LOGGING
#define log(...) log_internal(HPX_LOG_DEFAULT, __LINE__, __func__, __VA_ARGS__)
#define log_boot(...) log_internal(HPX_LOG_BOOT, __LINE__, __func__, __VA_ARGS__)
#define log_sched(...) log_internal(HPX_LOG_SCHED, __LINE__, __func__, __VA_ARGS__)
#define log_lco(...) log_internal(HPX_LOG_LCO, __LINE__, __func__, __VA_ARGS__)
#define log_gas(...) log_internal(HPX_LOG_GAS, __LINE__, __func__, __VA_ARGS__)
#define log_net(...) log_internal(HPX_LOG_NET, __LINE__, __func__, __VA_ARGS__)
#define log_trans(...) log_internal(HPX_LOG_TRANS, __LINE__, __func__, __VA_ARGS__)
#define log_parcel(...) log_internal(HPX_LOG_PARCEL, __LINE__, __func__, __VA_ARGS__)
#else
#define log(...)
#define log_boot(...)
#define log_sched(...)
#define log_lco(...)
#define log_gas(...)
#define log_net(...)
#define log_trans(...)
#define log_parcel(...)
#endif

#endif // LIBHPX_DEBUG_H
