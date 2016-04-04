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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/parcel.h>
#include <libhpx/rebalancer.h>
#include <libhpx/scheduler.h>
#include <libhpx/worker.h>
#include <uthash.h>
#include "agas.h"
#include "btt.h"
#include "gva.h"
#include "rebalancer.h"

// Block Statistics Table (BST) entry.
//
// The BST entry maintains statistics about block accesses. In
// particular, the number of times a block was accessed (@p counts)
// and the size of data transferred (@p sizes) is maintained. The
// index in the @p counts and @p sizes array represents the node which
// accessed this block. Each worker thread maintains its own private
// thread-local BST so that insertions into the BST don't have to be
// synchronized.
typedef struct agas_bst {
  uint64_t block;
  uint64_t *counts;
  uint64_t *sizes;
  UT_hash_handle hh;
} agas_bst_t;

// Per-locality BST.
//
// During statistics aggration, all of the thread-local BSTs are
// aggregated into a per-locality BST.
static void *_global_bst = NULL;

// Add an entry to the rebalancer's (thread-local) BST table.
///
/// @param      src The "src" locality accessing the block.
/// @param      dst The "dst" locality where the block is mapped.
/// @param    block The global address of the block.
/// @param     size The block's size in bytes.
///
//// @returns An error code, or HPX_SUCCESS.
void rebalancer_add_entry(int src, int dst, hpx_addr_t block, size_t size) {
  if (here->config->gas != HPX_GAS_AGAS) {
    return;
  }

  const agas_t *agas = here->gas;
  dbg_assert(agas && agas->btt);

  // ignore this block if it does not have the "load-balance"
  // (HPX_GAS_ATTR_LB) attribute
  gva_t gva = { .addr = block };
  uint32_t attr = 0;
  bool found = btt_get_attr(agas->btt, gva, &attr);
  if (likely(!found || !(attr & HPX_GAS_ATTR_LB))) {
    return;
  }

  // insert the block if it does not already exist
  agas_bst_t *entry = NULL;
  agas_bst_t **bst = (agas_bst_t**)&self->bst;
  HASH_FIND(hh, *bst, &block, sizeof(uint64_t), entry);
  if (!entry) {
    entry = malloc(sizeof(*entry));
    dbg_assert(entry);
    entry->block = block;
    entry->counts = calloc(here->ranks, sizeof(uint64_t));
    entry->sizes = calloc(here->ranks, sizeof(uint64_t));
    HASH_ADD(hh, *bst, block, sizeof(uint64_t), entry);
  }

  // otherwise, simply update the counts and sizes
  entry->counts[src]++;
  entry->sizes[src] += size;
}

// Initialize the AGAS-based rebalancer.
int rebalancer_init(void) {
  _global_bst = bst_new(0);
  dbg_assert(_global_bst);

  log_gas("GAS rebalancer initialized\n");
  return HPX_SUCCESS;
}

// Finalize the AGAS-based rebalancer.
void rebalancer_finalize(void) {
  if (_global_bst) {
    bst_delete(_global_bst);
    _global_bst = NULL;
  }
}

// This function takes the thread-local BST and merges it with the
// per-node global BST.
int _local_to_global_bst(int id, void *UNUSED) {
  worker_t *w = scheduler_get_worker(here->sched, id);
  dbg_assert(w);

  agas_bst_t **bst = (agas_bst_t **)&w->bst;
  if (*bst == NULL) {
    return HPX_SUCCESS;
  }

  log_gas("Added %u entries to global BST.\n", HASH_COUNT(*bst));
  agas_bst_t *entry, *tmp;
  HASH_ITER(hh, *bst, entry, tmp) {
    bst_upsert(_global_bst, entry->block, entry->counts, entry->sizes);
    HASH_DEL(*bst, entry);
    free(entry);
  }
  HASH_CLEAR(hh, *bst);
  *bst = NULL;
  return HPX_SUCCESS;
}

// Constructs a graph at the target global address from the serialized
// parcel payload.
static int _aggregate_global_bst_handler(void *data, size_t size) {
  hpx_addr_t graph = hpx_thread_current_target();
  const hpx_parcel_t *p = hpx_thread_current_parcel();
  return agas_graph_construct(graph, data, size, p->src);
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _aggregate_global_bst,
                     _aggregate_global_bst_handler, HPX_POINTER, HPX_SIZE_T);

// Aggregate the BST on a given locality.
//
// This function collects all of the block statistics on a given
// locality and serializes it into a parcel that is sent to the @p
// graph global address. All of the local BSTs are merged into a
// single global BST in parallel, before the global BST is serialized
// into a parcel. This function steals the current continuation and
// forwards it along to the generated parcel.
static int _aggregate_bst_handler(hpx_addr_t graph) {
  hpx_par_for_sync(_local_to_global_bst, 0, HPX_THREADS, NULL);
  hpx_parcel_t *p = NULL;
  size_t size = bst_serialize_to_parcel(_global_bst, &p);
  log_gas("Global BST size: %lu bytes.\n", size);
  if (!size) {
    return HPX_SUCCESS;
  }
  p->target = graph;
  p->action = _aggregate_global_bst;

  // *take* the current continuation
  hpx_parcel_t *this = scheduler_current_parcel();
  p->c_target = this->c_target;
  p->c_action = this->c_action;
  this->c_target = HPX_NULL;
  this->c_action = HPX_ACTION_NULL;

  hpx_parcel_send(p, HPX_NULL);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _aggregate_bst, _aggregate_bst_handler,
                     HPX_ADDR);


// Move blocks in bulk to their new owners.
static int
_bulk_move_handler(int n, void *args[], size_t sizes[]) {
  uint64_t      *vtxs = args[0];
  uint64_t *partition = args[1];

  size_t bytes = sizes[0];
  uint64_t count = bytes/sizeof(uint64_t);

  hpx_addr_t done = hpx_lco_and_new(count);
  for (int i = 0; i < count; ++i) {
    int new_owner = partition[i];
    if (new_owner != here->rank) {
      log_gas("move block 0x%lx from %d to %d\n", vtxs[i], here->rank, new_owner);
      EVENT_GAS_MOVE(vtxs[i], HPX_HERE, HPX_THERE(new_owner));
      hpx_gas_move(vtxs[i], HPX_THERE(new_owner), done);
    } else {
      hpx_lco_set(done, 0, NULL, HPX_NULL, HPX_NULL);
    }
  }
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED | HPX_VECTORED, _bulk_move,
                     _bulk_move_handler, HPX_INT, HPX_POINTER, HPX_POINTER);

// Start rebalancing the blocks.
// This can be called by any locality in the system.
static int _rebalance_sync(uint64_t *partition, hpx_addr_t graph, void *g) {
  // get the vertex array
  uint64_t *vtxs = NULL;
  size_t nvtxs = agas_graph_get_vtxs(g, &vtxs);
  if (nvtxs > 0 && partition) {
    // rebalance blocks in each partition
    hpx_addr_t done = hpx_lco_and_new(HPX_LOCALITIES);
    for (int i = 0; i < HPX_LOCALITIES; ++i) {
      int start, end, owner;
      agas_graph_get_owner_entry(g, i, &start, &end, &owner);
      size_t bytes = (end-start)*sizeof(uint64_t);
      hpx_call(HPX_THERE(owner), _bulk_move, done,
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
static LIBHPX_ACTION(HPX_DEFAULT, 0, _rebalance, _rebalance_sync,
                     HPX_POINTER, HPX_ADDR, HPX_POINTER);

// Start partitioning the aggregated BST.
//
static int _partition_sync(hpx_addr_t msync) {
  hpx_addr_t graph = hpx_thread_current_target();
  void *g = NULL;
  if (!hpx_gas_try_pin(graph, (void**)&g)) {
    return HPX_RESEND;
  }

  uint64_t *partition = NULL;
  size_t nvtxs = agas_graph_partition(g, here->ranks, &partition);
  log_gas("Finished partitioning block graph (%ld vertices)\n", nvtxs);
  return hpx_call(HPX_HERE, _rebalance, msync, &partition, &graph, &g);
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _partition, _partition_sync, HPX_ADDR);

// Aggregate the global BSTs.
//
static int _aggregate_sync(hpx_addr_t psync, hpx_addr_t msync) {
  log_gas("Starting GAS rebalancing\n");

  hpx_addr_t graph = agas_graph_new();
  // first, aggregate the "block" graph locally
  hpx_bcast_rsync(_aggregate_bst, &graph);
  log_gas("Block graph aggregated on locality %d\n", HPX_LOCALITY_ID);
  return hpx_call(graph, _partition, psync, &msync);
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _aggregate, _aggregate_sync, HPX_ADDR,
                     HPX_ADDR);

// Start rebalancing.
//
// The LCO @p sync can be used for completion notification. This can
// be called by any locality in the system.
int rebalancer_start(hpx_addr_t async, hpx_addr_t psync, hpx_addr_t msync) {
  if (here->config->gas != HPX_GAS_AGAS) {
    hpx_lco_set(async, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(psync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(msync, 0, NULL, HPX_NULL, HPX_NULL);
    return 0;
  }

  return hpx_call(HPX_HERE, _aggregate, async, &psync, &msync);
}
