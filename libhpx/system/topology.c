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

topology_t *topology_new(void) {
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

  // get the CPUs in the system
  topology->ncpus = hwloc_get_nbobjs_by_type(topology->hwloc_topology,
                                             HWLOC_OBJ_PU);
  topology->cpus = calloc(topology->ncpus, sizeof(hwloc_obj_t));
  if (!topology->cpus) {
    log_error("failed to allocate memory for cpu objects.\n");
    return NULL;
  }

  // initalize the NUMA map
  topology->numa_map = calloc(topology->ncpus, sizeof(int));
  if (!topology->numa_map) {
    log_error("failed to allocate memory for the NUMA map.\n");
    return NULL;
  }

  // get the NUMA nodes in the system
  topology->nnodes = hwloc_get_nbobjs_by_type(topology->hwloc_topology,
                                              HWLOC_OBJ_NODE);
  topology->numa_nodes = calloc(topology->nnodes, sizeof(hwloc_obj_t));
  if (!topology->numa_nodes) {
    log_error("failed to allocate memory for numa node objects.\n");
    return NULL;
  }

  hwloc_obj_t cpu = NULL;
  for (int i = 0; i < topology->ncpus; ++i) {
    cpu = hwloc_get_next_obj_by_type(topology->hwloc_topology,
                                     HWLOC_OBJ_PU, cpu);
    dbg_assert(cpu);
    topology->cpus[cpu->os_index] = cpu;
    hwloc_obj_t numa_node =
      hwloc_get_ancestor_obj_by_type(topology->hwloc_topology,
                                     HWLOC_OBJ_NODE, cpu);
    dbg_assert(numa_node);
    if (topology->numa_nodes[numa_node->os_index] == NULL) {
      topology->numa_nodes[numa_node->os_index] = numa_node;
    }
    topology->numa_map[cpu->os_index] = numa_node->os_index;
  }
  
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

  hwloc_topology_destroy(topology->hwloc_topology);
}
