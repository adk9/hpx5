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
#include <libhpx/scheduler.h>
#include <libhpx/system.h>
#include <libhpx/topology.h>
#include <libhpx/worker.h>
#include <hwloc.h>

static int _hwloc_bind_curthread(hwloc_bitmap_t set) {
  int e = hwloc_set_cpubind(here->topology->hwloc_topology,
                            set, HWLOC_CPUBIND_THREAD);
  if (e) {
    log_error("_hwloc_bind_curthread() failed with error %s.\n", strerror(e));
    return LIBHPX_ERROR;
  }

  return LIBHPX_OK;
}

int system_set_worker_affinity(int id, libhpx_thread_affinity_t policy) {
  int resource;
  int cpu = (id % here->topology->ncpus);
  switch (policy) {
   case HPX_THREAD_AFFINITY_DEFAULT:
   case HPX_THREAD_AFFINITY_NUMA:
     resource = here->topology->numa_map[cpu];
     break;
   case HPX_THREAD_AFFINITY_CORE:
     resource = here->topology->core_map[cpu];
     break;
   case HPX_THREAD_AFFINITY_HWTHREAD:
     resource = cpu;
     break;
   case HPX_THREAD_AFFINITY_NONE:
     resource = -1;
   default:
     log_error("unknown thread affinity policy\n");
     return LIBHPX_ERROR;
  }

  // if we didn't find a valid cpuset, we ignore affinity.
  if (resource < 0) {
    return LIBHPX_OK;
  }

  hwloc_cpuset_t cpuset = here->topology->cpu_affinity_map[resource];
  return _hwloc_bind_curthread(cpuset);
}

int system_get_available_cores(void) {
  return hwloc_bitmap_weight(here->topology->allowed_cpus);
}
