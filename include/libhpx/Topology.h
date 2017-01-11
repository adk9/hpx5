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

#ifndef LIBHPX_TOPOLOGY_CXX_H
#define LIBHPX_TOPOLOGY_CXX_H

#include <hwloc.h>
#include "libhpx/config.h"

#ifndef BUILD_HWLOC
#define libhpx_hwloc_topology_t hwloc_topology_t
#define libhpx_hwloc_obj_t hwloc_obj_t
#define libhpx_hwloc_cpuset_t hwloc_cpuset_t
#define libhpx_hwloc_set_cpubind hwloc_set_cpubind
#define libhpx_hwloc_get_cpubind hwloc_get_cpubind
#define libhpx_hwloc_bitmap_weight hwloc_bitmap_weight
#define libhpx_hwloc_bitmap_alloc hwloc_bitmap_alloc
#define libhpx_hwloc_bitmap_set hwloc_bitmap_set
#define libhpx_hwloc_bitmap_set_range hwloc_bitmap_set_range
#define libhpx_hwloc_bitmap_free hwloc_bitmap_free
#define libhpx_hwloc_bitmap_singlify hwloc_bitmap_singlify
#define libhpx_hwloc_topology_init hwloc_topology_init
#define libhpx_hwloc_topology_load hwloc_topology_load
#define libhpx_hwloc_topology_destroy hwloc_topology_destroy
#define libhpx_hwloc_get_nbobjs_by_type hwloc_get_nbobjs_by_type
#define libhpx_hwloc_get_next_obj_by_type hwloc_get_next_obj_by_type
#define libhpx_hwloc_get_ancestor_obj_by_type hwloc_get_ancestor_obj_by_type
#define LIBHPX_HWLOC_CPUBIND_THREAD HWLOC_CPUBIND_THREAD
#define LIBHPX_HWLOC_OBJ_PU HWLOC_OBJ_PU
#define LIBHPX_HWLOC_OBJ_CORE HWLOC_OBJ_CORE
#define LIBHPX_HWLOC_OBJ_NUMANODE HWLOC_OBJ_NODE
#define LIBHPX_HWLOC_CPUBIND_PROCESS HWLOC_CPUBIND_PROCESS
#else
#define LIBHPX_HWLOC_CPUBIND_THREAD LIBHPX_hwloc_CPUBIND_THREAD
#define LIBHPX_HWLOC_OBJ_PU LIBHPX_hwloc_OBJ_PU
#define LIBHPX_HWLOC_OBJ_CORE LIBHPX_hwloc_OBJ_CORE
#define LIBHPX_HWLOC_OBJ_NUMANODE LIBHPX_hwloc_OBJ_NUMANODE
#define LIBHPX_HWLOC_CPUBIND_PROCESS LIBHPX_hwloc_CPUBIND_PROCESS
#endif

// the number of bits for each part of the packed value
#define     TOPO_PE_BITS (16)
#define TOPO_WORKER_BITS (8)
#define TOPO_OFFSET_BITS (8 * sizeof(uint64_t) - TOPO_PE_BITS \
                          - TOPO_WORKER_BITS)

// shift values for the three parts of the packed value
#define     TOPO_PE_SHIFT (TOPO_WORKER_BITS + TOPO_OFFSET_BITS)
#define TOPO_WORKER_SHIFT (TOPO_OFFSET_BITS)
#define TOPO_OFFSET_SHIFT (0)

// masks to clobber bits of the address (use with &)
#define TOPO_LOCATION_MASK (UINT64_MAX << TOPO_WORKER_SHIFT)
#define       TOPO_PE_MASK (UINT64_MAX << TOPO_PE_SHIFT)
#define   TOPO_WORKER_MASK ((UINT64_MAX << TOPO_WORKER_SHIFT) & \
                            (~TOPO_PE_MASK))
#define   TOPO_OFFSET_MASK (~(TOPO_LOCATION_MASK))
#define  TOPO_MAX_LG_BSIZE (sizeof(uint32_t)*8)

namespace libhpx {

class Topology {
  public:
    Topology()
      : ncpus_(0),
        ncores_(0),
        nnodes_(0),
        numa_nodes_(nullptr),
        cpus_per_node_(0),
        cpu_to_core_(0),
        cpu_to_numa_(nullptr),
        numa_to_cpus_(0),
        cpu_affinity_map_(nullptr)
    {
    }

    ~Topology() {
    }

    /// Allocate and initialize a new topology object.
    ///
    /// @param   size_t
    ///          config The configuration object.
    ///
    /// @returns The topology object, or NULL if there was an error.
    static void* operator new(size_t, config_t *config);

    /// Deallocate the topology object.
    ///
    /// @param   topology The pointer to the topology object.
    static void operator delete(void*, Topology *topology);

    int getNnodes() const {
      return nnodes_;
    }

    int getNcpus() const {
      return ncpus_;
    }

    int getNcores() const {
      return ncores_;
    }

    int getCpuToNuma(int cpu) const {
      return cpu_to_numa_[cpu];
    }

    int getCpuToCore(int cpu) const {
      return cpu_to_core_[cpu];
    }

    int getCpusPerNode() const {
      return cpus_per_node_;
    }

    int getNumaToCpus(int node, int cpu) {
      return numa_to_cpus_[node][cpu];
    }

    int **getNumaToCpusPtr() {
      return numa_to_cpus_;
    }

    libhpx_hwloc_cpuset_t getCpuAffinityMap(int resource) {
      return cpu_affinity_map_[resource];
    }

    libhpx_hwloc_cpuset_t *getAffinityMap() {
      return cpu_affinity_map_;
    }

    libhpx_hwloc_topology_t getHwlocTopology() {
      return hwloc_topology_;
    }

    libhpx_hwloc_cpuset_t getAllowedCpus() {
      return allowed_cpus_;
    }

    /// Extract the locality from a packed value.
    static inline uint32_t topo_value_to_rank(uint64_t value) {
      return (value & TOPO_PE_MASK) >> TOPO_PE_SHIFT;
    }

    /// Extract the locality from a packed value.
    static inline uint32_t topo_value_to_worker(uint64_t value) {
      return (value & TOPO_WORKER_MASK) >> TOPO_WORKER_SHIFT;
    }

    /// Extract the offset value of a packed value, given the number of ranks.
    /// @param        value The tological, packed value
    ///
    /// @returns            The offset part of @p value
    static inline uint64_t topo_value_to_offset(uint64_t value) {
      return (value & TOPO_OFFSET_MASK) >> TOPO_OFFSET_SHIFT;
    }

    /// Create a topological, packed value from a locality and offset value
    ///
    /// @param     locality The locality to be packed with this value
    /// @param       worker The worker id to be packed with this value
    /// @param         offset The offset value to be packed with this packed value
    ///
    /// @returns            A packed value encoding the locality and the offset
    static inline uint64_t topo_offset_to_value(uint32_t locality, uint32_t worker,
                                                uint64_t offset)
    {
      uint64_t pe = (((uint64_t)locality) << TOPO_PE_SHIFT) & TOPO_PE_MASK;
      uint64_t core = (((uint64_t)worker) << TOPO_WORKER_SHIFT) & TOPO_WORKER_MASK;
      uint64_t dat = (offset << TOPO_OFFSET_SHIFT) & TOPO_OFFSET_MASK;
      return pe + core + dat;
   }

  private:
    libhpx_hwloc_topology_t  hwloc_topology_; //!< The HWLOC topology object.
    int                        ncpus_; //!< The number of physical CPUs in the
                                      //!< system.

    libhpx_hwloc_obj_t         *cpus_; //!< The HWLOC object corresponding to each
                                      //!< CPU.

    int                       ncores_; //!< The number of physical cores in the
                                      //!< system.

    int                       nnodes_; //!< The number of NUMA nodes in the system.
    libhpx_hwloc_obj_t   *numa_nodes_; //!< The HWLOC object corresponding to each
                                      //!< NUMA node.

    int                cpus_per_node_; //!< The number of CPUs per NUMA node
    int                 *cpu_to_core_; //!< The CPU to core mapping.
    int                 *cpu_to_numa_; //!< The CPU to NUMA node mapping.
    int               **numa_to_cpus_; //!< The (reverse) NUMA node to cpus mapping.
    libhpx_hwloc_cpuset_t      allowed_cpus_; //!< The initial CPU - HPX process binding.
    libhpx_hwloc_cpuset_t *cpu_affinity_map_; //!< The CPU affinity map that maintains the
                                      //!< CPU binding for a resource (numa-node,
                                      //!< core-id) depending on the global
                                      //!< affinity policy.
};
} // namespace libhpx

#endif // LIBHPX_TOPOLOGY_CXX_H
