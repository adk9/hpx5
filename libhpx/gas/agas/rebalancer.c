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
static agas_bst_t **_local_bsts;

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
  if (!_local_bsts) {
    _local_bsts = calloc(HPX_THREADS, sizeof(*_local_bsts));
  }

  if (!_global_bst) {
    _global_bst = bst_new(0);
  }

  dbg_assert(_global_bst);
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
  int id = HPX_THREAD_ID;
  dbg_assert(_local_bsts[id]);

  // publish my private bst to the global BST array
  _local_bsts[id] = _local_bst;
}

// This construct a sparse graph in the compressed storage format
// (CSR) from the thread-private block statistics table.
int _local_to_global_bst(int id, void *env) {
  agas_bst_t *bst = _local_bsts[id];
  agas_bst_t *entry, *tmp;

  HASH_ITER(hh, bst, entry, tmp) {
    bst_upsert(_global_bst, entry->block, entry->counts, entry->sizes);
    free(entry);
  }
  return HPX_SUCCESS;
}

static int _aggregate_global_bst_handler(void *data, size_t size) {
  hpx_addr_t graph = hpx_thread_current_target();
  return agas_graph_from_bst(graph, data, size);
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

typedef struct _rebalance_blocks__args {
  hpx_addr_t done;
  uint64_t *vtxs;
  uint64_t *partition;
} _rebalance_blocks_args_t;

int _rebalance_blocks(int id, void *a) {
  _rebalance_blocks_args_t *args = (_rebalance_blocks_args_t*)a;
  hpx_gas_move(args->vtxs[id], HPX_THERE(args->partition[id]), args->done);
  return HPX_SUCCESS;
}

// Start balancing the blocks.
// This can be called by any locality in the system.
static int libhpx_rebalancer_start_sync(void) {
  hpx_addr_t graph = agas_graph_new();

  // first, aggregate the "block" graph locally
  hpx_bcast_rsync(_aggregate_bst, &graph);

  void *g = NULL;
  if (!hpx_gas_try_pin(graph, (void**)&g)) {
    dbg_error("Could not pin the graph. Did it move?\n");
    return HPX_ERROR;
  }
  
  // then, divide it into partitions
  _rebalance_blocks_args_t args;
  int nvtxs = agas_graph_get_vtxs(g, &args.vtxs);
  args.done = hpx_lco_and_new(nvtxs);
  size_t psize = agas_graph_partition(g, here->ranks, &args.partition);
  dbg_assert(psize > 0 && args.partition);

  // rebalance blocks based on the resulting partition
  hpx_par_for(_rebalance_blocks, 0, psize, &args, HPX_NULL);
  hpx_lco_wait(args.done);
  hpx_lco_delete(args.done, HPX_NULL);

  hpx_gas_unpin(graph);
  free(args.partition);
  agas_graph_delete(graph);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _rebalancer_start_sync,
                     libhpx_rebalancer_start_sync);

int libhpx_rebalancer_start(hpx_addr_t sync) {
  if (here->config->gas != HPX_GAS_AGAS) {
    return 0;
  }
  return hpx_call(HPX_HERE, _rebalancer_start_sync, sync);
}
