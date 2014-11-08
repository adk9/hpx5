#define __STDC_FORMAT_MACROS

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>

#include "edge_list.h"
#include "libpxgl.h"
#include "hpx/hpx.h"
#include "libsync/sync.h"


static hpx_action_t _put_edge_edgelist;
static int _put_edge_edgelist_action(edge_list_edge_t *e)
{
   const hpx_addr_t target = hpx_thread_current_target();

   edge_list_edge_t *edge;
   if (!hpx_gas_try_pin(target, (void**)&edge))
     return HPX_RESEND;

   memcpy(edge, e, sizeof(*e));
   hpx_gas_unpin(target);
   return HPX_SUCCESS;
}


hpx_action_t edge_list_from_file = 0;
int edge_list_from_file_action(char **filename) {

  // Read from the edge-list filename
  FILE *f = fopen(*filename, "r");
  assert(f);

  edge_list_t *el = malloc(sizeof(*el));
  assert(el);
  el->min_edge_weight = UINT64_MAX;
  el->max_edge_weight = 0;

  edge_list_edge_t *edge = malloc(sizeof(*edge));
  assert(edge);

  hpx_addr_t edges = HPX_NULL;
  uint64_t count = 0;

  char line[LINE_MAX];
  while (fgets(line, sizeof(line), f) != NULL) {
    switch (line[0]) {
      case 'c': continue;
      case 'a':

        sscanf(&line[1], " %lu %lu %lu", &edge->source, &edge->dest, &edge->weight);
        if (el->min_edge_weight > edge->weight)
          el->min_edge_weight = edge->weight;

        if (el->max_edge_weight < edge->weight)
          el->max_edge_weight = edge->weight;

        sscanf(&line[1], " %" PRIu64 " %" PRIu64 " %" PRIu64, &edge->source, &edge->dest, &edge->weight);

        hpx_addr_t e = hpx_addr_add(el->edge_list, count * sizeof(edge_list_edge_t), el->edge_list_bsize);
        count++;
        assert(edges != HPX_NULL);
        hpx_call(e, _put_edge_edgelist, edge, sizeof(*edge), edges);
        break;

      case 'p':
        sscanf(&line[1], " sp %" PRIu64 " %" PRIu64, &el->num_vertices, &el->num_edges);

        // Account for the  DIMACS graph format (.gr) where the node
        // ids range from 1..n
        el->num_vertices++;

        // Set an appropriate block size
        el->edge_list_bsize = ((el->num_edges + HPX_LOCALITIES - 1) / HPX_LOCALITIES) * sizeof(edge_list_edge_t);

        // Allocate an edge_list array in the global address space
        el->edge_list = hpx_gas_global_alloc(HPX_LOCALITIES, el->edge_list_bsize);

        // Populate the edge_list
        edges = hpx_lco_and_new(el->num_edges);
        break;
      default:
        fprintf(stderr, "invalid command specifier '%c' in graph file. skipping..\n", line[0]);
        continue;
    }
  }

  assert(edges != HPX_NULL);
  hpx_lco_wait(edges);
  hpx_lco_delete(edges, HPX_NULL);
  free(edge);
  fclose(f);

  hpx_thread_continue_cleanup(sizeof(*el), el, free, el);
  return HPX_SUCCESS;
}


static __attribute__((constructor)) void _edge_list_register_actions() {
  _put_edge_edgelist  = HPX_REGISTER_ACTION(_put_edge_edgelist_action);
  edge_list_from_file = HPX_REGISTER_ACTION(edge_list_from_file_action);
}
