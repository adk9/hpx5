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
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <unistd.h>
#include "libsync/locks.h"
#include "hpx/hpx.h"

#include "libhpx/action.h"
#include "libhpx/config.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/debug.h"


// Used for debugging. Causes a process to wait for a debugger to
// attach, and set the value if i != 0.
HPX_NO_OPTIMIZE void dbg_wait(void) {
  int i = 0;
  char hostname[255];
  gethostname(hostname, 255);
  printf("PID %d on %s ready for attach\n", getpid(), hostname);
  fflush(stdout);
  while (0 == i)
    sleep(12);
}

static void __print(FILE *file, unsigned line, const char *filename,
                    const char *func, const char *fmt, va_list *list) {
  int tid = hpx_get_my_thread_id();
  int rank = hpx_get_my_rank();
  fprintf(file, "LIBHPX<%d,%d>: (%s:%s:%u) ", rank, tid, basename((char*)filename), func, line);
  vfprintf(file, fmt, *list);
  fflush(file);
}

/// Helper macro, extracts the va-args and forwards to __print.
#define _print(file, line, filename, func, fmt)      \
  do {                                               \
    va_list args;                                    \
    va_start(args, fmt);                             \
    __print(file, line, filename, func, fmt, &args); \
    va_end(args);                                    \
  } while (0)

void dbg_error_internal(unsigned line, const char *filename, const char *func,
                       const char *fmt, ...) {
  _print(stderr, line, filename, func, fmt);
  hpx_abort();
}

uint64_t log_level = HPX_LOG_DEFAULT;

static tatas_lock_t _log_lock = SYNC_TATAS_LOCK_INIT;

void log_internal(unsigned line, const char *filename, const char *func,
                  const char *fmt, ...) {
  sync_tatas_acquire(&_log_lock);
  _print(stdout, line, filename, func, fmt);
  sync_tatas_release(&_log_lock);
}

int log_error_internal(unsigned line, const char *filename, const char *func,
                       const char *fmt, ...) {
  sync_tatas_acquire(&_log_lock);
  _print(stderr, line, filename, func, fmt);
  sync_tatas_release(&_log_lock);
  return HPX_ERROR;
}

/// This is unsafe because we can't use gethostname or printf in a signal
/// handler, particularly a SEGV handler.
static void dbg_wait_on_segv(int signum) {
  dbg_wait();
}

int dbg_init(config_t *config) {
  if (DEBUG && config->dbg_waitonsegv) {
    if (SIG_ERR == signal(SIGSEGV, dbg_wait_on_segv)) {
      log_error("could not register dbg_wait_on_segv for SIGSEGV\n");
      return LIBHPX_ERROR;
    }
    else {
      log("registered dbg_wait_on_segv for SIGSEGV\n");
    }
  }
  return LIBHPX_OK;
}

void dbg_fini(void) {
}
