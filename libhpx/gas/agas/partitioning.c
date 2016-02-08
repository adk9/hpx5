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

// A graph representing the GAS accesses.
typedef struct agas_graph {
  int nvtxs;
  int nedges;
  UT_string *vtxs;
  UT_string *vwgt;
  UT_string *vsizes;
  UT_string *xadj;
  UT_string *adjncy;
  UT_string *adjwgt;
  volatile int lock;
} _agas_graph_t;

#define _UTBUF(s) ((void*)utstring_body(s))

static void _init(_agas_graph_t *graph) {
  // Initialize locks.
  sync_store(&graph->lock, 1, SYNC_RELEASE);

  graph->nvtxs = 0;
  graph->nedges = 0;
  utstring_new(graph->vtxs);
  utstring_new(graph->vwgt);
  utstring_new(graph->vsizes);
  utstring_new(graph->xadj);
  utstring_new(graph->adjncy);
  utstring_new(graph->adjwgt);  
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

static void _free(_agas_graph_t *g) {
  utstring_free(g->vtxs);
  utstring_free(g->vwgt);
  utstring_free(g->vsizes);
  utstring_free(g->xadj);
  utstring_free(g->adjncy);
  utstring_free(g->adjwgt);
  free(g);
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

int agas_graph_from_bst(hpx_addr_t graph, uint64_t *data, size_t size) {
  _agas_graph_t *g = NULL;

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

  if (!hpx_gas_try_pin(graph, (void**)&g)) {
    dbg_error("Could not pin the graph. Did it move?\n");
    return HPX_ERROR;
  }  

  while (!sync_swap(&g->lock, 0, SYNC_ACQUIRE))
    ;

  g->nvtxs += nvtxs;
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
  dbg_assert(sizeof(idx_t) == sizeof(uint64_t));

  idx_t options[METIS_NOPTIONS];
  options[METIS_OPTION_NUMBERING] = 0;

  idx_t ncon = 1;
  idx_t objval;
  *partition = calloc(g->nvtxs, sizeof(uint64_t));
  dbg_assert(partition);
  METIS_PartGraphRecursive(&g->nvtxs, &ncon, _UTBUF(g->xadj), _UTBUF(g->adjncy),
                           _UTBUF(g->vwgt), _UTBUF(g->vsizes), _UTBUF(g->adjwgt),
                           &nparts, NULL, NULL, options, &objval, (idx_t*)*partition);
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
