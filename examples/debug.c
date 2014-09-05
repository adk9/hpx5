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

#include <unistd.h>
#include <stdio.h>
#include <hpx/hpx.h>
#include "debug.h"

static void _wait(void) {
  int i = 0;
  char hostname[256];
  gethostname(hostname, sizeof(hostname));
  printf("PID %d on %s ready for attach\n", getpid(), hostname);
  fflush(stdout);
  while (0 == i)
    sleep(12);
}

/**
 * Used for debugging. Causes a process to wait for a debugger to attach, and
 * set the value if i != 0.
 */
void wait_for_debugger(int rank) {
  if ((rank == ALL_RANKS) || (rank == hpx_get_my_rank()))
    _wait();

  return;
}
