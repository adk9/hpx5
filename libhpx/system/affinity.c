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
#include <libhpx/topology.h>
#include <hwloc.h>

static int _hwloc_cpubind(hwloc_thread_t thread, hwloc_bitmap_t set) {
  int e = hwloc_set_thread_cpubind(here->topology->hwloc_topology,
                                   thread, set, HWLOC_CPUBIND_THREAD);
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

int system_get_available_cores(void) {
  return hwloc_bitmap_weight(here->topology->allowed_cpus);
}
