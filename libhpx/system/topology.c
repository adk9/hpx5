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
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/system.h>
#include <libhpx/topology.h>
#include <libhpx/utils.h>
#include <hwloc.h>

static int _get_resources_by_affinity(topology_t *topology,
                                      libhpx_thread_affinity_t policy) {
  int n = 0;
  switch (policy) {
   case HPX_THREAD_AFFINITY_DEFAULT:
   case HPX_THREAD_AFFINITY_NUMA:
     n = topology->nnodes;
     break;
   case HPX_THREAD_AFFINITY_CORE:
     n = topology->ncores;
     break;
   case HPX_THREAD_AFFINITY_HWTHREAD:
     n = topology->ncpus;
     break;
   case HPX_THREAD_AFFINITY_NONE:
     return 0;
   default:
     log_error("unknown thread affinity policy\n");
     return 0;
  }
  return n;
}

static hwloc_cpuset_t *_cpu_affinity_map_new(topology_t *topology, 
                                             libhpx_thread_affinity_t policy) {
  int resources = _get_resources_by_affinity(topology, policy);
  if (!resources) {
    return NULL;
  }
  hwloc_cpuset_t *cpu_affinity_map = calloc(resources, sizeof(*cpu_affinity_map));

  for (int r = 0; r < resources; ++r) {
    hwloc_cpuset_t cpuset = cpu_affinity_map[r] = hwloc_bitmap_alloc();  
    switch (policy) {
     case HPX_THREAD_AFFINITY_DEFAULT:
     case HPX_THREAD_AFFINITY_NUMA:
       for (int i = 0; i < topology->ncpus; ++i) {
         if (r == topology->numa_map[i]) {
           hwloc_bitmap_set(cpuset, i);
         }
       }
       break;
     case HPX_THREAD_AFFINITY_CORE:
       for (int i = 0; i < topology->ncpus; ++i) {
         if (r == topology->core_map[i]) {
           hwloc_bitmap_set(cpuset, i);
         }
       }
       break;
     case HPX_THREAD_AFFINITY_HWTHREAD:
       hwloc_bitmap_set(cpuset, r);
       hwloc_bitmap_singlify(cpuset);
       break;
     case HPX_THREAD_AFFINITY_NONE:
       hwloc_bitmap_set_range(cpuset, 0, topology->ncpus);       
       break;
     default:
       log_error("unknown thread affinity policy\n");
       return NULL;
    }
  }
  return cpu_affinity_map;
}

static void _cpu_affinity_map_delete(topology_t *topology) {
  int resources = _get_resources_by_affinity(topology, here->config->thread_affinity);
  for (int r = 0; r < resources; ++r) {
    hwloc_bitmap_free(topology->cpu_affinity_map[r]);
  }
  free(topology->cpu_affinity_map);
}

topology_t *topology_new(const struct config *config) {
  topology_t *topology = malloc(sizeof(*topology));
  int e = hwloc_topology_init(&topology->hwloc_topology);
  if (e) {
    log_error("failed to initialize HWLOC topology.\n");
    return NULL;
  }
  e = hwloc_topology_load(topology->hwloc_topology);
  if (e) {
    log_error("failed to load HWLOC topology.\n");
    return NULL;
  }

  // get the number of CPUs in the system
  topology->ncpus = hwloc_get_nbobjs_by_type(topology->hwloc_topology,
                                             HWLOC_OBJ_PU);

  // detect how many CPUs we can run on based on affinity or
  // environment information. on cray platforms, we look at the ALPS
  // depth to figure out how many CPUs to use
  topology->allowed_cpus = hwloc_bitmap_alloc();
  if (!topology->allowed_cpus) {
    log_error("failed to allocate memory for cpuset.\n");
    return NULL;
  }

  int cores = libhpx_getenv_num("ALPS_APP_DEPTH", 0);
  if (!cores) {
    e = hwloc_get_cpubind(topology->hwloc_topology, topology->allowed_cpus,
                          HWLOC_CPUBIND_PROCESS);
    if (e) {
      // failed to get the CPU binding, use all available cpus
      hwloc_bitmap_set_range(topology->allowed_cpus, 0, topology->ncpus);
    }
  } else {
    hwloc_bitmap_set_range(topology->allowed_cpus, 0, cores);
  }

  topology->cpus = calloc(topology->ncpus, sizeof(hwloc_obj_t));
  if (!topology->cpus) {
    log_error("failed to allocate memory for cpu objects.\n");
    return NULL;
  }

  // get the number of cores in the system
  topology->ncores = hwloc_get_nbobjs_by_type(topology->hwloc_topology,
                                              HWLOC_OBJ_CORE);
  // initalize the core map
  topology->core_map = calloc(topology->ncpus, sizeof(int));
  if (!topology->core_map) {
    log_error("failed to allocate memory for the core map.\n");
    return NULL;
  }

  // initalize the NUMA map
  topology->numa_map = calloc(topology->ncpus, sizeof(int));
  if (!topology->numa_map) {
    log_error("failed to allocate memory for the NUMA map.\n");
    return NULL;
  }

  // get the number of NUMA nodes in the system
  topology->nnodes = hwloc_get_nbobjs_by_type(topology->hwloc_topology,
                                              HWLOC_OBJ_NODE);
  topology->numa_nodes = NULL;
  if (topology->nnodes > 0) {
    topology->numa_nodes = calloc(topology->nnodes, sizeof(hwloc_obj_t));
    if (!topology->numa_nodes) {
      log_error("failed to allocate memory for numa node objects.\n");
      return NULL;
    }
  }

  hwloc_obj_t cpu = NULL;
  for (int i = 0; i < topology->ncpus; ++i) {
    cpu = hwloc_get_next_obj_by_type(topology->hwloc_topology,
                                     HWLOC_OBJ_PU, cpu);
    dbg_assert(cpu);
    topology->cpus[cpu->os_index] = cpu;

    hwloc_obj_t core =
      hwloc_get_ancestor_obj_by_type(topology->hwloc_topology,
                                     HWLOC_OBJ_CORE, cpu);
    int index = core ? core->os_index : -1;
    topology->core_map[cpu->os_index] = index;

    hwloc_obj_t numa_node =
      hwloc_get_ancestor_obj_by_type(topology->hwloc_topology,
                                     HWLOC_OBJ_NODE, cpu);
    index = numa_node ? numa_node->os_index : -1;
    if (numa_node && topology->numa_nodes[index] == NULL) {
      topology->numa_nodes[index] = numa_node;
    }
    topology->numa_map[cpu->os_index] = index;
  }

  // generate the CPU affinity map
  topology->cpu_affinity_map = _cpu_affinity_map_new(topology, config->thread_affinity);
  return topology;
}

void topology_delete(topology_t *topology) {
  if (!topology) {
    return;
  }

  if (topology->cpus) {
    free(topology->cpus);
    topology->cpus = NULL;
  }

  if (topology->numa_nodes) {
    free(topology->numa_nodes);
    topology->numa_nodes = NULL;
  }

  if (topology->numa_map) {
    free(topology->numa_map);
    topology->numa_map = NULL;
  }

  if (topology->allowed_cpus) {
    hwloc_bitmap_free(topology->allowed_cpus);
    topology->allowed_cpus = NULL;
  }

  if (topology->cpu_affinity_map) {
    _cpu_affinity_map_delete(topology);
    topology->cpu_affinity_map  = NULL;
  }

  hwloc_topology_destroy(topology->hwloc_topology);
  free(topology);
}
