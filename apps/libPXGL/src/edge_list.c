
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "edge_list.h"
#include "libpxgl.h"
#include "hpx/hpx.h"
#include "libsync/sync.h"


#ifndef _EDGE_LIST_BLOCKS
#define _EDGE_LIST_BLOCKS HPX_LOCALITIES
#endif
#ifndef _EDGE_LIST_BLOCK_SIZE
#define _EDGE_LIST_BLOCK_SIZE(n) (((n * sizeof(edge_list_edge_t)) + HPX_LOCALITIES - 1) / HPX_LOCALITIES)
#endif


static hpx_action_t _put_edge;
static int _put_edge_action(edge_list_edge_t *e)
{
   const hpx_addr_t target = hpx_thread_current_target();

   edge_list_edge_t *edge;
   if (!hpx_gas_try_pin(target, (void**)&edge))
     return HPX_RESEND;

   memcpy(edge, e, sizeof(*e));
   hpx_gas_unpin(target);
   return HPX_SUCCESS;
}


hpx_action_t edge_list_from_file = NULL;
int edge_list_from_file_action(char **filename) {

  // Read from the edge-list filename
  FILE *f = fopen(*filename, "r");
  assert(f);

  edge_list_t *el = malloc(sizeof(*el));
  assert(el);

  fscanf(f, "%lu", &el->num_vertices);
  fscanf(f, "%lu", &el->num_edges);

  // Allocate an edge_list array in the global address space
  el->edge_list = hpx_gas_global_alloc(_EDGE_LIST_BLOCKS, _EDGE_LIST_BLOCK_SIZE(el->num_edges));

  edge_list_edge_t *edge = malloc(sizeof(*edge));
  assert(edge);

  // Populate the edge_list
  hpx_addr_t edges = hpx_lco_and_new(el->num_edges);

  for (int i = 0; i < el->num_edges; i++){
    fscanf(f, "%lu %lu %lu", &edge->source, &edge->dest, &edge->weight);
    hpx_addr_t e = hpx_addr_add(el->edge_list, i * sizeof(edge_list_edge_t));
    hpx_call(e, _put_edge, edge, sizeof(*edge), edges);
  }
  hpx_lco_wait(edges);
  hpx_lco_delete(edges, HPX_NULL);
  free(edge);

  hpx_thread_continue(sizeof(*el), el);
  return HPX_SUCCESS;
}


static __attribute__((constructor)) void _edge_list_register_actions() {
  _put_edge           = HPX_REGISTER_ACTION(_put_edge_action);
  edge_list_from_file = HPX_REGISTER_ACTION(edge_list_from_file_action);
}
