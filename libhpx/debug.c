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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <limits.h>
#include <stdarg.h>
#include <unistd.h>
#include "libsync/locks.h"
#include "hpx/hpx.h"

#include "libhpx/config.h"
#include "libhpx/locality.h"
#include "libhpx/debug.h"


// Used for debugging. Causes a process to wait for a debugger to
// attach, and set the value if i != 0.
HPX_OPTIMIZE("O0")
void dbg_wait(void) {
  int i = 0;
  char hostname[255];
  gethostname(hostname, 255);
  printf("PID %d on %s ready for attach\n", getpid(), hostname);
  fflush(stdout);
  while (0 == i)
    sleep(12);
}

static void __print(FILE *file, unsigned line, const char *filename,
                    const char *fmt, va_list *list) {
  int tid = hpx_get_my_thread_id();
  int rank = hpx_get_my_rank();
  fprintf(file, "LIBHPX<%d,%d>: (%s:%u) ", rank, tid, filename, line);
  vfprintf(file, fmt, *list);
  fflush(file);
}

/// Helper macro, extracts the va-args and forwards to __print.
#define _print(file, line, filename, fmt)       \
  do {                                          \
    va_list args;                               \
    va_start(args, fmt);                        \
    __print(file, line, filename, fmt, &args);  \
    va_end(args);                               \
  } while (0)

int dbg_error_internal(unsigned line, const char *filename, const char *fmt,
                       ...) {
  _print(stderr, line, filename, fmt);
  hpx_abort();
  return HPX_ERROR;
}

void dbg_assert_str_internal(bool e, unsigned line, const char *filename,
                             const char *fmt, ...) {
  if (e) {
    return;
  }

  _print(stderr, line, filename, fmt);
  hpx_abort();
}

hpx_log_t log_level = HPX_LOG_DEFAULT;

static tatas_lock_t _log_lock = SYNC_TATAS_LOCK_INIT;

void log_internal(const hpx_log_t level, unsigned line, const char *filename,
                  const char *fmt, ...) {
  if (!(log_level & level)) {
    return;
  }

  sync_tatas_acquire(&_log_lock);
  _print(stdout, line, filename, fmt);
  sync_tatas_release(&_log_lock);
}
