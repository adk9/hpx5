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
#include <uthash.h>
#include "agas.h"
#include "btt.h"
#include "gva.h"
#include "rebalancer.h"

// Block Statistics Table (BST) entry.
typedef struct agas_bst {
  uint64_t block;
  uint64_t *counts;
  uint64_t *sizes;
  UT_hash_handle hh;
} agas_bst_t;

// BST private to a worker thread
static __thread agas_bst_t *_local_bst;

// Global array of all BSTs
static agas_bst_t ***_local_bsts;

// Per-locality BST
static void *_global_bst;

// Add an entry to the thread-local BST.
void libhpx_rebalancer_add_entry(int src, int dst, hpx_addr_t block,
                                 size_t size) {
  if (here->config->gas != HPX_GAS_AGAS) {
    return;
  }
  const agas_t *agas = here->gas;
  dbg_assert(agas && agas->btt);

  // ignore this block if it does not have the "load-balance"
  // attribute
  gva_t gva = { .addr = block };
  uint32_t attr = btt_get_attr(agas->btt, gva);
  if (!(attr & HPX_GAS_ATTR_LB)) {
    return;
  }
  
  agas_bst_t *entry = NULL;
  HASH_FIND_INT(_local_bst, (uint64_t*)&block, entry);
  if (!entry) {
    entry = malloc(sizeof(*entry));
    dbg_assert(entry);
    entry->counts = calloc(here->ranks, sizeof(uint64_t));
    entry->sizes = calloc(here->ranks, sizeof(uint64_t));
    HASH_ADD_INT(_local_bst, block, entry);
    return;
  }
  entry->counts[src]++;
  entry->sizes[src] += size;
}

// Initialize the AGAS-based rebalancer
int libhpx_rebalancer_init(void) {
  _local_bsts = calloc(here->config->threads, sizeof(agas_bst_t**));
  dbg_assert(_local_bsts);

  _global_bst = bst_new(0);
  dbg_assert(_global_bst);

  log_gas("GAS rebalancer initialized\n");
  return HPX_SUCCESS;
}

void libhpx_rebalancer_finalize(void) {
  free(_local_bsts);
  _local_bsts = NULL;

  bst_delete(_global_bst);
  _global_bst = NULL;
}

void libhpx_rebalancer_bind_worker(void) {
  if (here->config->gas != HPX_GAS_AGAS) {
    return;
  }

  // publish my private bst to the global BST array
  _local_bsts[HPX_THREAD_ID] = &_local_bst;
}

// This construct a sparse graph in the compressed storage format
// (CSR) from the thread-private block statistics table.
int _local_to_global_bst(int id, void *env) {
  agas_bst_t *bst = *_local_bsts[id];
  dbg_assert(bst);

  agas_bst_t *entry, *tmp;
  HASH_ITER(hh, bst, entry, tmp) {
    bst_upsert(_global_bst, entry->block, entry->counts, entry->sizes);
    free(entry);
  }
  return HPX_SUCCESS;
}

// Constructs a graph at the target global address from the serialized
// parcel payload.
static int _aggregate_global_bst_handler(void *data, size_t size) {
  hpx_addr_t graph = hpx_thread_current_target();
  const hpx_parcel_t *p = hpx_thread_current_parcel();
  return agas_graph_from_bst(graph, data, size, p->src);
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _aggregate_global_bst,
                     _aggregate_global_bst_handler, HPX_POINTER, HPX_SIZE_T);

static int _aggregate_bst_handler(hpx_addr_t graph) {
  hpx_par_for_sync(_local_to_global_bst, 0, HPX_THREADS, NULL);
  hpx_parcel_t *p = NULL;
  size_t size = bst_serialize_to_parcel(_global_bst, &p);
  dbg_assert(size > 0);
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

int _rebalance_blocks_handler(int start, int end, int owner, void *graph,
                              uint64_t *partition, hpx_addr_t done) {
  uint64_t *vtxs = NULL;
  agas_graph_get_vtxs(graph, &vtxs);

  for (int i = start; i <= end; ++i) {
    int new_owner = partition[i];
    if (owner != new_owner) {
      log_gas("move block 0x%lx from %d to %d (0x%lx)\n", vtxs[i], owner,
              new_owner, HPX_THERE(new_owner));
      hpx_gas_move(vtxs[i], HPX_THERE(new_owner), done);
    } else {
      hpx_lco_set(done, 0, NULL, HPX_NULL, HPX_NULL);
    }
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _rebalance_blocks,
                     _rebalance_blocks_handler, HPX_INT, HPX_INT, HPX_INT,
                     HPX_POINTER, HPX_POINTER, HPX_ADDR);

// Start balancing the blocks.
// This can be called by any locality in the system.
static int libhpx_rebalancer_start_sync(void) {
  log_gas("Starting GAS rebalancing\n");

  hpx_addr_t graph = agas_graph_new();
  // first, aggregate the "block" graph locally
  hpx_bcast_rsync(_aggregate_bst, &graph);

  void *g = NULL;
  if (!hpx_gas_try_pin(graph, (void**)&g)) {
    dbg_error("Could not pin the graph. Did it move?\n");
    return HPX_ERROR;
  }
  
  // then, divide it into partitions
  uint64_t *partition = NULL;
  size_t nvtxs = agas_graph_partition(g, here->ranks, &partition);
  dbg_assert(nvtxs > 0 && partition);

  // rebalance blocks based on the resulting partition
  hpx_addr_t done = hpx_lco_and_new(nvtxs);
  for (int i = 0; i < HPX_LOCALITIES; ++i) {
    int start, end, owner;
    agas_graph_get_owner_entry(g, i, &start, &end, &owner);
    hpx_call(HPX_HERE, _rebalance_blocks, HPX_NULL, &start, &end, &owner,
             &g, &partition, &done);
  }
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  hpx_gas_unpin(graph);
  free(partition);
  agas_graph_delete(graph);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _rebalancer_start_sync,
                     libhpx_rebalancer_start_sync);

int libhpx_rebalancer_start(hpx_addr_t sync) {
  if (here->config->gas != HPX_GAS_AGAS) {
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
    return 0;
  }
  return hpx_call(HPX_HERE, _rebalancer_start_sync, sync);
}
