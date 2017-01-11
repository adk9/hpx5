// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#include "libhpx/Topology.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/system.h"
#include "libhpx/util/Env.h"
#include <alloca.h>
#include <string.h>
#include <unistd.h>
#include <hwloc.h>

namespace {
  using namespace libhpx;
}

static int _get_resources_by_affinity(Topology *topology,
                                      libhpx_thread_affinity_t policy) {
  int n = 0;
  switch (policy) {
   default:
     log_error("unknown thread affinity policy\n");
   case HPX_THREAD_AFFINITY_DEFAULT:
   case HPX_THREAD_AFFINITY_NONE:
     return 0;
   case HPX_THREAD_AFFINITY_NUMA:
     n = topology->getNnodes();
     break;
   case HPX_THREAD_AFFINITY_CORE:
     n = topology->getNcores();
     break;
   case HPX_THREAD_AFFINITY_HWTHREAD:
     n = topology->getNcpus();
     break;
  }
  return n;
}

static libhpx_hwloc_cpuset_t *_cpu_affinity_map_new(Topology *topology,
                                             libhpx_thread_affinity_t policy) {
  int resources = _get_resources_by_affinity(topology, policy);
  if (!resources) {
    return NULL;
  }
  libhpx_hwloc_cpuset_t *cpu_affinity_map = new libhpx_hwloc_cpuset_t[resources];

  for (int r = 0; r < resources; ++r) {
    libhpx_hwloc_cpuset_t cpuset = cpu_affinity_map[r] = libhpx_hwloc_bitmap_alloc();
    switch (policy) {
     case HPX_THREAD_AFFINITY_DEFAULT:
     case HPX_THREAD_AFFINITY_NUMA:
       for (int i = 0; i < topology->getNcpus(); ++i) {
         if (r == topology->getCpuToNuma(i)) {
           libhpx_hwloc_bitmap_set(cpuset, i);
         }
       }
       break;
     case HPX_THREAD_AFFINITY_CORE:
       for (int i = 0; i < topology->getNcpus(); ++i) {
         if (r == topology->getCpuToCore(i)) {
           libhpx_hwloc_bitmap_set(cpuset, i);
         }
       }
       break;
     case HPX_THREAD_AFFINITY_HWTHREAD:
       libhpx_hwloc_bitmap_set(cpuset, r);
       libhpx_hwloc_bitmap_singlify(cpuset);
       break;
     case HPX_THREAD_AFFINITY_NONE:
       libhpx_hwloc_bitmap_set_range(cpuset, 0, topology->getNcpus()-1);
       break;
     default:
       log_error("unknown thread affinity policy\n");
       return NULL;
    }
  }
  return cpu_affinity_map;
}

static void _cpu_affinity_map_delete(Topology *topology) {
  int resources = _get_resources_by_affinity(topology, here->config->thread_affinity);
  for (int r = 0; r < resources; ++r) {
    libhpx_hwloc_bitmap_free(topology->getCpuAffinityMap(r));
  }
  delete [] topology->getAffinityMap();
}

void 
Topology::operator delete(void *, Topology* topology) {
  if (!topology) {
    return;
  }

  if (topology->cpus_) {
    delete [] topology->cpus_;
    topology->cpus_ = NULL;
  }

  if (topology->numa_nodes_) {
    delete [] topology->numa_nodes_;
    topology->numa_nodes_ = NULL;
  }
  
  if (topology->cpu_to_core_) {
    delete [] topology->cpu_to_core_;
    topology->cpu_to_core_ = NULL;
  }

  if (topology->cpu_to_numa_) {
    delete [] topology->cpu_to_numa_;
    topology->cpu_to_numa_ = NULL;
  }

  if (topology->numa_to_cpus_) {
    for (int i = 0; i < topology->nnodes_; ++i) {
      if (topology->numa_to_cpus_[i]) {
        delete [] topology->numa_to_cpus_[i];
      }
    }
    delete [] topology->numa_to_cpus_;
    topology->numa_to_cpus_ = NULL;
  }

  if (topology->allowed_cpus_) {
    libhpx_hwloc_bitmap_free(topology->allowed_cpus_);
    topology->allowed_cpus_ = NULL;
  }

  if (topology->cpu_affinity_map_) {
    _cpu_affinity_map_delete(topology);
    topology->cpu_affinity_map_  = NULL;
  }

  libhpx_hwloc_topology_destroy(topology->hwloc_topology_);
  ::operator delete(topology);
}

static void*
Topology::operator new(size_t, config_t *config) {
  Topology *topo = ::new Topology; // Use the default allocator

  // Initialize the hwloc infrastructure.
  libhpx_hwloc_topology_t *hwloc = &topo->hwloc_topology_;
  if (libhpx_hwloc_topology_init(hwloc)) {
    log_error("failed to initialize HWLOC topology.\n");
    ::operator delete(NULL, topo);
    return NULL;
  }

  if (libhpx_hwloc_topology_load(*hwloc)) {
    log_error("failed to load HWLOC topology.\n");
    ::operator delete(NULL, topo);
    return NULL;
  }

  // Query the hwloc object for cpus, cores, and numa nodes---"fix" nnodes if
  // hwloc returns something unpleasant
  topo->ncpus_ = libhpx_hwloc_get_nbobjs_by_type(*hwloc, LIBHPX_HWLOC_OBJ_PU);
  topo->ncores_ = libhpx_hwloc_get_nbobjs_by_type(*hwloc, LIBHPX_HWLOC_OBJ_CORE);
  topo->nnodes_ = libhpx_hwloc_get_nbobjs_by_type(*hwloc, LIBHPX_HWLOC_OBJ_NUMANODE);
  topo->nnodes_ = (topo->nnodes_ > 0) ? topo->nnodes_ : 1;
  topo->cpus_per_node_ = topo->ncpus_ / topo->nnodes_;

  // Allocate our secondary arrays.
  topo->allowed_cpus_ = libhpx_hwloc_bitmap_alloc();
  if (!topo->allowed_cpus_) {
    log_error("failed to allocate memory for cpuset.\n");
    ::operator delete(NULL, topo);
    return NULL;
  }

  topo->cpus_ = new libhpx_hwloc_obj_t[topo->ncpus_]();
  if (!topo->cpus_) {
    log_error("failed to allocate memory for cpu objects.\n");
    ::operator delete(NULL, topo);
    return NULL;
  }

  topo->cpu_to_core_ = new int[topo->ncpus_]();
  if (!topo->cpu_to_core_) {
    log_error("failed to allocate memory for the core map.\n");
    ::operator delete(NULL, topo);
    return NULL;
  }

  topo->cpu_to_numa_ = new int[topo->ncpus_]();
  if (!topo->cpu_to_numa_) {
    log_error("failed to allocate memory for the NUMA map.\n");
    ::operator delete(NULL, topo);
    return NULL;
  }

  topo->numa_nodes_ = new libhpx_hwloc_obj_t[topo->nnodes_]();
  if (!topo->numa_nodes_) {
    log_error("failed to allocate memory for numa node objects.\n");
    ::operator delete(NULL, topo);
    return NULL;
  }

  topo->numa_to_cpus_ = new int*[topo->nnodes_]();
  if (!topo->numa_to_cpus_) {
    log_error("failed to allocate memory for the reverse NUMA map.\n");
    ::operator delete(NULL, topo);
    return NULL;
  }

  for (int i = 0, e = topo->nnodes_; i < e; ++i) {
    topo->numa_to_cpus_[i] = new int[topo->cpus_per_node_]();
    if (!topo->numa_to_cpus_[i]) {
      log_error("failed to allocate memory for the reverse NUMA map.\n");
      ::operator delete(NULL, topo);
      return NULL;
    }
  }

  // Detect how many CPUs we can run on based on affinity or environment.  On
  // aprun platforms, we look at the ALPS depth to figure out how many CPUs to
  // use, otherwise we try and use hwloc to figure out what the process mask is
  // set to, otherwise we just bail out and use all CPUs.
  int cores = 0;
  try {
    cores = libhpx::util::getEnv<int>("ALPS_APP_DEPTH");
  }
  catch (const libhpx::util::NotFound&) {
    // Check for the SLURM launcher.
    try {
      cores = libhpx::util::getEnv<int>("SLURM_CPUS_PER_TASK");
    }
    catch (const libhpx::util::NotFound&) { }
  }

  if (cores > 0) {
    libhpx_hwloc_bitmap_set_range(topo->allowed_cpus_, 0, cores - 1);
  }
  else if (libhpx_hwloc_get_cpubind(*hwloc, topo->allowed_cpus_, LIBHPX_HWLOC_CPUBIND_PROCESS))
  {
    libhpx_hwloc_bitmap_set_range(topo->allowed_cpus_, 0, topo->ncpus_ - 1);
  }

  // We want to be able to map from a numa domain to its associated cpus, but we
  // are going to iterate through the cpus in an undefined order. This array
  // keeps track of the "next" index for each numa node array, so that we can
  // insert into the right place.
  int numa_to_cpus_next[topo->nnodes_];
  memset(numa_to_cpus_next, 0, sizeof(numa_to_cpus_next));

  libhpx_hwloc_obj_t cpu = NULL;
  libhpx_hwloc_obj_t core = NULL;
  libhpx_hwloc_obj_t numa_node = NULL;
  for (int i = 0, e = topo->ncpus_; i < e; ++i) {
    // get the hwloc tree nodes for the cpu, core, and numa node
    cpu = libhpx_hwloc_get_next_obj_by_type(*hwloc, LIBHPX_HWLOC_OBJ_PU, cpu);
    core = libhpx_hwloc_get_ancestor_obj_by_type(*hwloc, LIBHPX_HWLOC_OBJ_CORE, cpu);
    numa_node = libhpx_hwloc_get_ancestor_obj_by_type(*hwloc, LIBHPX_HWLOC_OBJ_NUMANODE, cpu);

    // get integer indexes for the cpu, core, and numa node
    int index = cpu->os_index;
    int core_index = (core) ? core->os_index : 0;
    #if defined(__PPC64__)
      int numa_index = (numa_node) ? numa_node->logical_index : 0;
    #else
      int numa_index = (numa_node) ? numa_node->os_index : 0;
    #endif

    // record our hwloc nodes so that we can get them quickly during queries
    topo->cpus_[index] = cpu;
    topo->numa_nodes_[numa_index] = numa_node;

    // record our core and numa indices for quick query
    topo->cpu_to_core_[index] = core_index;
    topo->cpu_to_numa_[index] = numa_index;

    // and record this cpu in the correct numa set
    int next_cpu_index = numa_to_cpus_next[numa_index]++;
    topo->numa_to_cpus_[numa_index][next_cpu_index] = index;
  }

  // generate the CPU affinity map
  topo->cpu_affinity_map_ = _cpu_affinity_map_new(topo, config->thread_affinity);
  return topo;
}
