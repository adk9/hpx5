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

#ifndef LIBHPX_GAS_AGAS_REBALANCER_H
#define LIBHPX_GAS_AGAS_REBALANCER_H

#include "BlockStatisticsTable.h"
#include "hpx/hpx.h"

namespace libhpx {
namespace gas {
namespace agas {

class Rebalancer {
  using BST = libhpx::gas::agas::HierarchicalBST;
 public:
  Rebalancer();
  ~Rebalancer();

  static Rebalancer* Instance() {
    dbg_assert(Instance_);
    return Instance_;
  }

  /// Record an entry in the rebalancer's BST.
  ///
  /// @param      src The "src" locality accessing the block.
  /// @param      dst The "dst" locality where the block is mapped.
  /// @param    block The global address of the block.
  /// @param     size The block's size in bytes.
  void record(int src, int dst, hpx_addr_t block, size_t size);

  // Start rebalancing asynchronously.
  int start(hpx_addr_t async, hpx_addr_t psync, hpx_addr_t msync);

  static int AggregateHandler(hpx_addr_t psync, hpx_addr_t msync) {
    return Instance()->aggregate(psync, msync);
  }

  static int PartitionHandler(hpx_addr_t msync) {
    return Instance()->partition(msync);
  }

  static int MoveHandler(uint64_t* partition, hpx_addr_t graph, void* g) {
    return Instance()->move(partition, graph, g);
  }

  static int SerializeBSTHandler(hpx_addr_t graph) {
    return Instance()->serializeBST(graph);
  }

 private:
  int aggregate(hpx_addr_t psync, hpx_addr_t msync);
  int partition(hpx_addr_t msync);
  int move(uint64_t* partition, hpx_addr_t graph, void* g);
  int serializeBST(hpx_addr_t graph);
  
  static Rebalancer* Instance_;
  BST bst_;
};

} // namespace agas
} // namespace gas
} // namespace libhpx

// AGAS Graph Partitioning API
hpx_addr_t agas_graph_new(void);
void agas_graph_delete(hpx_addr_t graph);
int agas_graph_construct(hpx_addr_t graph, void* input, size_t size, int owner);
size_t agas_graph_partition(void *g, int nparts, uint64_t **partition);
size_t agas_graph_get_vtxs(void *graph, uint64_t **vtxs);
void agas_graph_get_owner_entry(void *graph, unsigned id, int *start,
                                int *end, int *owner);

#endif // LIBHPX_GAS_AGAS_REBALANCER_H
