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

hpx_log_t dbg_log_level = HPX_LOG_DEFAULT;

void dbg_log1(unsigned line, const char *f, const hpx_log_t level,
              const char *fmt, ...) {
  static tatas_lock_t lock = SYNC_TATAS_LOCK_INIT;
  if (dbg_log_level & level) {
    int tid = here ? hpx_get_my_thread_id() : -1;
    int rank = here ? hpx_get_my_rank() : -1;
    sync_tatas_acquire(&lock);
    printf("LIBHPX<%d,%d>: (%s:%u) ", rank, tid, f, line);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
    sync_tatas_release(&lock);
  }
}

int
dbg_error1(unsigned line, const char *f, const char *fmt, ...) {
  int tid = here ? hpx_get_my_thread_id() : -1;
  int rank = here ? hpx_get_my_rank() : -1;
  fprintf(stderr, "LIBHPX<%d,%d>: (%s:%u) ", rank, tid, f, line);

  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  fflush(stderr);
  hpx_abort();
  return HPX_ERROR;
}


/**
 * Used for debugging. Causes a process to wait for a debugger to attach, and
 * set the value if i != 0.
 */
HPX_OPTIMIZE("O0")
void dbg_wait(void) {
  int i = 0;
  char hostname[HOST_NAME_MAX];
  gethostname(hostname, HOST_NAME_MAX);
  printf("PID %d on %s ready for attach\n", getpid(), hostname);
  fflush(stdout);
  while (0 == i)
    sleep(12);
}
