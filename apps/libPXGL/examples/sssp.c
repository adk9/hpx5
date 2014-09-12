// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "hpx/hpx.h"
#include "libpxgl.h"


static void _usage(FILE *stream) {
  fprintf(stream, "Usage: sssp [options] <edge-list-graph> SOURCE\n"
          "\t-c, number of cores to run on\n"
          "\t-t, number of scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, this help display\n");
}


static hpx_action_t _print_vertex_distance;
static int _print_vertex_distance_action(void *args)
{
  const hpx_addr_t target = hpx_thread_current_target();

  adj_list_vertex_t *vertex;
  if (!hpx_gas_try_pin(target, (void**)&vertex))
    return HPX_RESEND;

  fprintf(stderr, "%lu\n", vertex->distance);

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


static hpx_action_t _print_vertex_distance_index;
static int _print_vertex_distance_index_action(void *args)
{
  const hpx_addr_t target = hpx_thread_current_target();

  hpx_addr_t *v;
  hpx_addr_t vertex; 
  
  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);

  return hpx_call_sync(vertex, _print_vertex_distance, NULL, 0, NULL, 0);
}


// Arguments for the main SSSP action
typedef struct {
  char *filename;
  uint64_t source;
} _sssp_args_t;


static hpx_action_t _main;
static int _main_action(_sssp_args_t *args) {

  // Create an edge list structure from the given filename
  edge_list_t el;
  hpx_call_sync(HPX_HERE, edge_list_from_file, &args->filename, sizeof(char*), &el, sizeof(el));

  // Construct the graph as an adjacency list
  call_sssp_args_t sargs = { .graph = HPX_NULL, .source = args->source };
  hpx_call_sync(HPX_HERE, adj_list_from_edge_list, &el, sizeof(el), &sargs.graph, sizeof(sargs.graph));

  // Call the SSSP algorithm
  hpx_call_sync(HPX_HERE, call_sssp, &sargs, sizeof(sargs), NULL, 0);


  // Verification of results.

  // Action to print the distances of each vertex from the source
  hpx_addr_t vertices = hpx_lco_and_new(el.num_vertices);
  for (int i = 0; i < el.num_vertices; ++i) {
    hpx_addr_t index = hpx_addr_add(sargs.graph, i * sizeof(hpx_addr_t));
    hpx_call(index, _print_vertex_distance_index, NULL, 0, vertices);
  }
  hpx_lco_wait(vertices);
  hpx_lco_delete(vertices, HPX_NULL);
  return HPX_SUCCESS;
}


int main(int argc, char *const argv[argc]) {
  hpx_config_t cfg = {
    .cores       = 0,
    .threads     = 0,
    .stack_bytes = 0
  };

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:Dh")) != -1) {
    switch (opt) {
     case 'c':
      cfg.cores = atoi(optarg);
      break;
     case 't':
      cfg.threads = atoi(optarg);
      break;
     case 'D':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = HPX_LOCALITY_ALL;
      break;
     case 'd':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = atoi(optarg);
      break;
     case 'h':
      _usage(stdout);
      return 0;
     case '?':
     default:
      _usage(stderr);
      return -1;
    }
  }

  argc -= optind;
  argv += optind;

  char *filename;
  uint64_t source;

  switch (argc) {
   case 0:
    fprintf(stderr, "\nMissing edge-list-graph file.\n");
    _usage(stderr);
    return -1;
    case 1:
    fprintf(stderr, "\nMissing SOURCE vertex.\n");
    _usage(stderr);
    return -1;
   default:
    _usage(stderr);
    return -1;
   case 2:
     filename = argv[0];
     source = atoi(argv[1]);
     break;
  }

  int e = hpx_init(&cfg);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  // register the actions
  _print_vertex_distance_index = HPX_REGISTER_ACTION(_print_vertex_distance_index_action);
  _print_vertex_distance       = HPX_REGISTER_ACTION(_print_vertex_distance_action);
  _main                        = HPX_REGISTER_ACTION(_main_action);

  _sssp_args_t args = { .filename = filename, .source = source };
  return hpx_run(_main, &args, sizeof(args));
}
