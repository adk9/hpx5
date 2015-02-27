// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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
#include "libhpx/libhpx.h"
#include "libhpx/system.h"
#include "hpx/hpx.h"

int system_get_cores(void) {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

int system_set_affinity(pthread_t thread, int core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  if (core_id == -1) {
    for (int e = 0; e < system_get_cores(); e++) {
      CPU_SET(e, &cpuset);
    }
  } else {
    CPU_SET(core_id, &cpuset);
  }
  int e = pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset);
  if (e) {
    log("failed to bind thread affinity.\n");
    return LIBHPX_ERROR;
  }
  return LIBHPX_OK;
}

int system_set_affinity_group(pthread_t thread, int ncores) {
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(ncores, &cpu_set);
  int e = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpu_set);
  if (e) {
    log("failed to bind thread affinity.\n");
    return LIBHPX_ERROR;
  }
  return LIBHPX_OK;
}
