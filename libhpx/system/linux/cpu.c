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
#ifndef __linux__
#error The HPX CPU implementation is configured incorrectly
#endif

#define _GNU_SOURCE /* pthread_setaffinity_np */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include "libhpx/debug.h"
#include "libhpx/system.h"
#include "hpx/hpx.h"

int system_get_cores(void) {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

int system_set_affinity(pthread_t thread, int core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  int e = pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
  if (e) // not fatal
    return dbg_error("system: failed to bind thread affinity.\n");
  return HPX_SUCCESS;
}
