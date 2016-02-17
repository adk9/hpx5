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
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libsync/sync.h>
#include <utstring.h>
#include "agas.h"
#include "btt.h"
#include "gva.h"

#ifdef HAVE_METIS
#include <metis.h>
#endif

// The owner map for vertices in the graph.
typedef struct _owner_map {
  uint64_t start; //!< the starting vertex id of vertices mapped to an owner
  int      owner;
} _owner_map_t;

// A graph representing the GAS accesses.
typedef struct agas_graph {
  int nvtxs;
  int nedges;
  _owner_map_t *owner_map;
  UT_string *vtxs;
  UT_string *vwgt;
  UT_string *vsizes;
  UT_string *xadj;
  UT_string *adjncy;
  UT_string *adjwgt;
  volatile int lock;
  int count;
} _agas_graph_t;

#define _UTBUF(s) ((void*)utstring_body(s))

static void _add_locality_nodes(_agas_graph_t *g) {
  int n = HPX_LOCALITIES;
  uint64_t vtxs[n];
  uint64_t vwgt[n];
  for (int i = 0; i < n; ++i) {
    vtxs[i] = i;
    vwgt[i] = INT_MAX;
  }
  uint64_t *zeros = calloc(n, sizeof(zeros));

  g->nvtxs = n;
  utstring_bincpy(g->vtxs, vtxs, sizeof(uint64_t)*n);
  utstring_bincpy(g->vwgt, vwgt, sizeof(uint64_t)*n);
  utstring_bincpy(g->vsizes, zeros, sizeof(uint64_t)*n);
  utstring_bincpy(g->xadj, zeros, sizeof(uint64_t)*n);
}

static void _init(_agas_graph_t *g) {
  // Initialize locks.
  sync_store(&g->lock, 1, SYNC_RELEASE);

  g->nvtxs = 0;
  g->nedges = 0;
  g->count = 0;
  g->owner_map = calloc(HPX_LOCALITIES, sizeof(*g->owner_map));
  utstring_new(g->vtxs);
  utstring_new(g->vwgt);
  utstring_new(g->vsizes);
  utstring_new(g->xadj);
  utstring_new(g->adjncy);
  utstring_new(g->adjwgt);

  _add_locality_nodes(g);
}

static void _free(_agas_graph_t *g) {
  utstring_free(g->vtxs);
  utstring_free(g->vwgt);
  utstring_free(g->vsizes);
  utstring_free(g->xadj);
  utstring_free(g->adjncy);
  utstring_free(g->adjwgt);
  free(g->owner_map);
  free(g);
}

hpx_addr_t agas_graph_new(void) {
  _agas_graph_t *g = NULL;
  hpx_addr_t graph = hpx_gas_alloc_local(1, sizeof(*g), 0);
  if (!hpx_gas_try_pin(graph, (void**)&g)) {
    dbg_error("Could not pin newly allocated process.\n");
  }
  dbg_assert(graph != HPX_NULL);
  dbg_assert(g);
  _init(g);
  hpx_gas_unpin(graph);
  return graph;
}

HPX_ACTION_DECL(agas_graph_delete_action);
void agas_graph_delete(hpx_addr_t graph) {
  _agas_graph_t *g = NULL;
  if (!hpx_gas_try_pin(graph, (void**)&g)) {
    int e = hpx_call(graph, agas_graph_delete_action, HPX_NULL, &graph);
    dbg_check(e, "Could not forward agas_graph_delete\n");
    return;
  }
  _free(g);
  hpx_gas_unpin(graph);
}
LIBHPX_ACTION(HPX_INTERRUPT, 0, agas_graph_delete_action,
              agas_graph_delete, HPX_ADDR);

static void _deserialize_bst(uint64_t *data, size_t size, int *nvtxs,
                             int *nedges, uint64_t **vtxs, uint64_t **vwgt,
                             uint64_t **vsizes, uint64_t **xadj,
                             uint64_t **adjncy, uint64_t **adjwgt) {
  *nvtxs = data[0];
  *vtxs = &data[1];
  *vwgt = &data[(*nvtxs)+1];
  *vsizes = &data[2*(*nvtxs)+1];
  *xadj = &data[3*(*nvtxs)+1];

  *nedges = data[4*(*nvtxs)+2];
  *adjncy = &data[4*(*nvtxs)+3];
  *adjwgt = &data[4*(*nvtxs)+3+(*nedges)];
}

int agas_graph_from_bst(hpx_addr_t graph, uint64_t *data, size_t size,
                        int owner) {
  int nvtxs;
  int nedges;
  uint64_t *vtxs;
  uint64_t *vwgt;
  uint64_t *vsizes;
  uint64_t *xadj;
  uint64_t *adjncy;
  uint64_t *adjwgt;

  _deserialize_bst(data, size, &nvtxs, &nedges, &vtxs, &vwgt, &vsizes,
                   &xadj, &adjncy, &adjwgt);

  _agas_graph_t *g = NULL;
  if (!hpx_gas_try_pin(graph, (void**)&g)) {
    dbg_error("Could not pin the graph. Did it move?\n");
    return HPX_ERROR;
  }

  while (!sync_swap(&g->lock, 0, SYNC_ACQUIRE))
    ;

  _owner_map_t *map = &g->owner_map[g->count++];
  map->start = g->nvtxs;
  map->owner = owner;

  g->nvtxs  += nvtxs;
  g->nedges += nedges;

  utstring_bincpy(g->vtxs, vtxs, sizeof(*vtxs)*nvtxs);
  utstring_bincpy(g->vwgt, vwgt, sizeof(*vwgt)*nvtxs);
  utstring_bincpy(g->vsizes, vsizes, sizeof(*vsizes)*nvtxs);
  utstring_bincpy(g->xadj, xadj, sizeof(*xadj)*(nvtxs+1));
  utstring_bincpy(g->adjncy, adjncy, sizeof(*adjncy)*nedges);
  utstring_bincpy(g->adjwgt, adjwgt, sizeof(*adjwgt)*nedges);

  sync_store(&g->lock, 1, SYNC_RELEASE);
  hpx_gas_unpin(graph);
  return HPX_SUCCESS;
}

#ifdef HAVE_METIS
static size_t _metis_partition(_agas_graph_t *g, int nparts,
                               uint64_t **partition) {
  idx_t options[METIS_NOPTIONS] = { 0 };
  options[METIS_OPTION_NUMBERING] = 0;
  options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;
  options[METIS_OPTION_CTYPE] = METIS_CTYPE_RM;
  options[METIS_OPTION_NCUTS] = 1;
  options[METIS_OPTION_NITER] = 10;
  options[METIS_OPTION_UFACTOR] = 1;

  idx_t ncon = 1;
  idx_t objval = 0;
  *partition = calloc(g->nvtxs, sizeof(uint64_t));
  dbg_assert(partition);
  int e = METIS_PartGraphRecursive(&g->nvtxs, &ncon, _UTBUF(g->xadj),
            _UTBUF(g->adjncy), _UTBUF(g->vwgt), _UTBUF(g->vsizes),
            _UTBUF(g->adjwgt), &nparts, NULL, NULL, options, &objval,
            (idx_t*)*partition);
  if (e != METIS_OK) {
    return 0;
  }
  return g->nvtxs;
}
#endif

size_t agas_graph_partition(void *g, int nparts, uint64_t **partition) {
#ifdef HAVE_METIS
  _agas_graph_t *graph = (_agas_graph_t*)g;
  return _metis_partition(graph, nparts, partition);
#else
  log_dflt("No partitioner found.\n");
#endif
  return 0;
}

size_t agas_graph_get_vtxs(void *graph, uint64_t **vtxs) {
  _agas_graph_t *g = (_agas_graph_t*)graph;
  dbg_assert(g);
  *vtxs = _UTBUF(g->vtxs);
  return g->nvtxs;
}

size_t agas_graph_get_owner_count(void *graph) {
  _agas_graph_t *g = (_agas_graph_t*)graph;
  return g->count;
}

int agas_graph_get_owner_entry(void *graph, uint64_t id, int *start,
                               int *end, int *owner) {
  _agas_graph_t *g = (_agas_graph_t*)graph;
  dbg_assert(g);
  dbg_assert(id >= 0 && id <= g->count);

  _owner_map_t entry = g->owner_map[id];
  *start = entry.start;
  *owner = entry.owner;

  if (id == g->count-1) {
    *end = g->nvtxs - 1;
  } else {
    _owner_map_t next_entry = g->owner_map[id+1];
    *end = next_entry.start - 1;
  }
  return HPX_SUCCESS;
}

