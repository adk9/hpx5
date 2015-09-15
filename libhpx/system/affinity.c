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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <unistd.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/system.h>
#include <libhpx/utils.h>
#include <hwloc.h>

int system_get_job_cpus(void) {
  // todo: detect which system we're on.

  // Cray ALPS
  int ncpus = libhpx_getenv_num("ALPS_APP_DEPTH", 0);

  // ..otherwise, use all available cores
  if (!ncpus) {
    ncpus = system_get_cores();
  }

  return ncpus;
}

static int _hwloc_cpubind(hwloc_thread_t thread, hwloc_bitmap_t set) {
  int e = hwloc_set_thread_cpubind(here->topology, thread,
                                   set, HWLOC_CPUBIND_THREAD);
  if (e) {
    log_error("_hwloc_cpubind() failed with error %s.\n", strerror(e));
    return LIBHPX_ERROR;
  }

  return LIBHPX_OK;
}

int system_set_affinity(pthread_t thread, int id) {
  hwloc_bitmap_t cpu_set = hwloc_bitmap_alloc();
  hwloc_bitmap_set(cpu_set, id);
  int e = _hwloc_cpubind(thread, cpu_set);
  hwloc_bitmap_free(cpu_set);
  return e;
}

int system_set_affinity_group(pthread_t thread, int ncores) {
  hwloc_bitmap_t cpu_set = hwloc_bitmap_alloc();
  hwloc_bitmap_set_range(cpu_set, 0, ncores);
  int e = _hwloc_cpubind(thread, cpu_set);
  hwloc_bitmap_free(cpu_set);
  return e;
}

int system_get_affinity_group_size(pthread_t thread, int *ncores) {
  hwloc_bitmap_t cpu_set = hwloc_bitmap_alloc();
  int e = hwloc_get_thread_cpubind(here->topology, (hwloc_thread_t)thread,
                                   cpu_set, HWLOC_CPUBIND_THREAD);
  if (e) {
    *ncores = 0;
  } else {
    *ncores = hwloc_bitmap_weight(cpu_set);
  }

  hwloc_bitmap_free(cpu_set);
  return LIBHPX_OK;
}
