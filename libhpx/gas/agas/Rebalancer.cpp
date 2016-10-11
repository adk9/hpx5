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

#include "GlobalVirtualAddress.h"
#include "Rebalancer.h"
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/parcel.h>
#include <libhpx/Scheduler.h>
#include <libhpx/Worker.h>
#include <unordered_map>

namespace {
using libhpx::self;
using libhpx::gas::agas::Rebalancer;
using GVA = libhpx::gas::agas::GlobalVirtualAddress;
using BST = libhpx::gas::agas::HierarchicalBST;
LIBHPX_ACTION(HPX_DEFAULT, 0, Aggregate, Rebalancer::AggregateHandler,
              HPX_ADDR, HPX_ADDR);
LIBHPX_ACTION(HPX_DEFAULT, 0, Partition, Rebalancer::PartitionHandler,
              HPX_ADDR);
LIBHPX_ACTION(HPX_DEFAULT, 0, Move, Rebalancer::MoveHandler,
              HPX_POINTER, HPX_ADDR, HPX_POINTER);
LIBHPX_ACTION(HPX_DEFAULT, 0, SerializeBST, Rebalancer::SerializeBSTHandler,
              HPX_ADDR);
LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED | HPX_VECTORED, BulkMove,
              Rebalancer::BulkMoveHandler, HPX_INT, HPX_POINTER, HPX_POINTER);
}

Rebalancer* Rebalancer::Instance_;

Rebalancer::Rebalancer()
    : bst_()
{
  dbg_assert(!Instance_);
  Instance_ = this;
}

Rebalancer::~Rebalancer()
{
}

void
Rebalancer::record(int src, int dst, hpx_addr_t block, size_t size) {
  if (here->config->gas != HPX_GAS_AGAS) {
    return;
  }

  // ignore this block if it does not have the "load-balance"
  // (HPX_GAS_ATTR_LB) attribute
  GVA gva(block);
  uint32_t attr = here->gas->getAttribute(gva);
  if (likely(!(attr & HPX_GAS_ATTR_LB))) {
    return;
  }

  // add an entry to the BST
  bst_.add(gva, src, 1, size);
}

int
Rebalancer::rebalance(hpx_addr_t async, hpx_addr_t psync, hpx_addr_t msync)
{
  log_gas("Starting GAS rebalancing\n");
  return hpx_call(HPX_HERE, Aggregate, async, &psync, &msync);
}

/// Constructs a graph at the target global address from the serialized
/// parcel payload.
static int _aggregate_global_bst_handler(void *data, size_t size) {
  const hpx_parcel_t *p = self->getCurrentParcel();
  return agas_graph_construct(p->target, data, size, p->src);
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _aggregate_global_bst,
                     _aggregate_global_bst_handler, HPX_POINTER, HPX_SIZE_T);

int
Rebalancer::aggregate(hpx_addr_t psync, hpx_addr_t msync)
{
  hpx_addr_t graph = agas_graph_new();
  // first, aggregate the "block" graph locally
  hpx_bcast_rsync(SerializeBST, &graph);
  log_gas("Block graph aggregated on locality %d\n", HPX_LOCALITY_ID);
  return hpx_call(graph, Partition, psync, &msync);
}

/// Serialize the per-locality BST.
///
/// This function serializes the BST on a locality into a parcel that
/// is then sent to the @p graph. This function steals the current
/// continuation and forwards it along to the generated parcel.
int
Rebalancer::serializeBST(hpx_addr_t graph)
{
  hpx_parcel_t* p = bst_.toParcel();
  log_gas("Global BST size: %u bytes.\n", p->size);
  if (!p || !p->size) {
    return HPX_SUCCESS;
  }
  p->target = graph;
  p->action = _aggregate_global_bst;

  // *take* the current continuation
  hpx_parcel_t* curr = self->getCurrentParcel();
  p->c_target = curr->c_target;
  p->c_action = curr->c_action;
  curr->c_target = HPX_NULL;
  curr->c_action = HPX_ACTION_NULL;

  return hpx_parcel_send(p, HPX_NULL);
}

int
Rebalancer::partition(hpx_addr_t msync)
{
  hpx_addr_t graph = hpx_thread_current_target();
  void *g = NULL;
  if (!hpx_gas_try_pin(graph, (void**)&g)) {
    return HPX_RESEND;
  }

  uint64_t *partition = NULL;
  size_t nvtxs = agas_graph_partition(g, here->ranks, &partition);
  log_gas("Finished partitioning block graph (%ld vertices)\n", nvtxs);
  return hpx_call(HPX_HERE, Move, msync, &partition, &graph, &g);
}

int
Rebalancer::move(uint64_t *partition, hpx_addr_t graph, void *g)
{
  // get the vertex array
  uint64_t *vtxs = NULL;
  size_t nvtxs = agas_graph_get_vtxs(g, &vtxs);
  if (nvtxs > 0 && partition) {
    // rebalance blocks in each partition
    hpx_addr_t done = hpx_lco_and_new(here->ranks);
    for (unsigned i = 0; i < here->ranks; ++i) {
      int start, end, owner;
      agas_graph_get_owner_entry(g, i, &start, &end, &owner);
      size_t bytes = (end-start)*sizeof(uint64_t);
      if (!bytes) {
        hpx_lco_set(done, 0, NULL, HPX_NULL, HPX_NULL);
        continue;
      }
      hpx_call(HPX_THERE(owner), BulkMove, done,
               vtxs+start, bytes, partition+start, bytes);
    }
    hpx_lco_wait(done);
    hpx_lco_delete(done, HPX_NULL);
  }

  hpx_gas_unpin(graph);
  free(partition);
  agas_graph_delete(graph);
  return HPX_SUCCESS;
}

// Move blocks in bulk to their new owners.
int
BulkMoveHandler(int n, void *args[], size_t sizes[])
{
  hpx_addr_t *vtxs = static_cast<hpx_addr_t*>(args[0]);
  uint64_t *partition = static_cast<uint64_t*>(args[1]);

  size_t bytes = sizes[0];
  uint64_t count = bytes/sizeof(uint64_t);

  hpx_addr_t done = hpx_lco_and_new(count);
  for (unsigned i = 0; i < count; ++i) {
    unsigned newOwner = partition[i];
    if (newOwner != here->rank) {
      hpx_gas_move(vtxs[i], HPX_THERE(new_owner), done);
    } else {
      hpx_lco_set(done, 0, NULL, HPX_NULL, HPX_NULL);
    }
  }
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  return HPX_SUCCESS;
}
