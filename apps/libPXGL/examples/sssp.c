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
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <inttypes.h>

#include "hpx/hpx.h"
#include "libpxgl.h"


static void _usage(FILE *stream) {
  fprintf(stream, "Usage: sssp [options] <graph-file> <problem-file>\n"
          "\t-c, number of cores to run on\n"
          "\t-t, number of scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, this help display\n");
}


static hpx_action_t _print_vertex_distance;
static int _print_vertex_distance_action(int *i)
{
  const hpx_addr_t target = hpx_thread_current_target();

  adj_list_vertex_t *vertex;
  if (!hpx_gas_try_pin(target, (void**)&vertex))
    return HPX_RESEND;

  printf("vertex: %d nbrs: %lu dist: %llu\n", *i, vertex->num_edges, vertex->distance);

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


static hpx_action_t _print_vertex_distance_index;
static int _print_vertex_distance_index_action(int *i)
{
  const hpx_addr_t target = hpx_thread_current_target();

  hpx_addr_t *v;
  hpx_addr_t vertex;

  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);

  return hpx_call_sync(vertex, _print_vertex_distance, i, sizeof(*i), NULL, 0);
}


static int read_dimacs_spec(char **filename, uint64_t *nproblems, uint64_t **problems) {

  FILE *f = fopen(*filename, "r");
  assert(f);

  char line[LINE_MAX];
  int count = 0;
  while (fgets(line, sizeof(line), f) != NULL) {
    switch (line[0]) {
      case 'c': continue;
      case 's':
        sscanf(&line[1], " %llu", &((*problems)[count++]));
        break;
      case 'p':
        sscanf(&line[1], " aux sp ss %llu", nproblems);
        *problems = malloc(*nproblems * sizeof(uint64_t));
        assert(*problems);
        break;
      default:
        fprintf(stderr, "invalid command specifier '%c' in problem file. skipping..\n", line[0]);
        continue;
    }
  }
  fclose(f);
  return 0;
}


// Arguments for the main SSSP action
typedef struct {
  char *filename;
  uint64_t nproblems;
  uint64_t *problems;
} _sssp_args_t;


static hpx_action_t _main;
static int _main_action(_sssp_args_t *args) {

  // Create an edge list structure from the given filename
  edge_list_t el;
  printf("Allocated edge-list from file %s.\n", args->filename);
  hpx_call_sync(HPX_HERE, edge_list_from_file, &args->filename, sizeof(char*), &el, sizeof(el));
  printf("Edge List: #v = %llu, #e = %llu\n",
         el.num_vertices, el.num_edges);

  call_sssp_args_t sargs;
  for (int i = 0; i < args->nproblems; ++i) {
    // Construct the graph as an adjacency list
    hpx_call_sync(HPX_HERE, adj_list_from_edge_list, &el, sizeof(el), &sargs.graph, sizeof(sargs.graph));

    printf("Allocated adjacency-list.\n");

    sargs.source = args->problems[i];

    hpx_time_t now = hpx_time_now();
    
    // Call the SSSP algorithm
    hpx_call_sync(HPX_HERE, call_sssp, &sargs, sizeof(sargs), NULL, 0);

    double elapsed = hpx_time_elapsed_ms(now)/1e3;

    printf("Finished executing SSSP (chaotic-relaxation) in %.7f seconds.\n", elapsed);

    hpx_addr_t checksum_lco = HPX_NULL;
    hpx_call_sync(sargs.graph, dimacs_checksum, &el.num_vertices, sizeof(el.num_vertices), &checksum_lco, sizeof(checksum_lco));
    size_t checksum = 0;
    hpx_lco_get(checksum_lco, sizeof(checksum), &checksum);
    hpx_lco_delete(checksum_lco, HPX_NULL);
    printf("Dimacs checksum is %zu\n", checksum);

    hpx_gas_free(sargs.graph, HPX_NULL);
  }

  // Verification of results.
  printf("Verifying results...\n");

  // Action to print the distances of each vertex from the source
  hpx_addr_t vertices = hpx_lco_and_new(el.num_vertices-1);
  for (int i = 1; i < el.num_vertices; ++i) {
    hpx_addr_t index = hpx_addr_add(sargs.graph, i * sizeof(hpx_addr_t));
    hpx_call(index, _print_vertex_distance_index, &i, sizeof(i), vertices);
  }
  hpx_lco_wait(vertices);
  hpx_lco_delete(vertices, HPX_NULL);

  hpx_shutdown(HPX_SUCCESS);
  return HPX_SUCCESS;
}


int main(int argc, char *const argv[argc]) {
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;

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

  char *graph_file;
  char *problem_file;

  switch (argc) {
   case 0:
    fprintf(stderr, "\nMissing graph (.gr) file.\n");
    _usage(stderr);
    return -1;
   case 1:
    fprintf(stderr, "\nMissing problem specification (.ss) file.\n");
    _usage(stderr);
    return -1;
   default:
    _usage(stderr);
    return -1;
   case 2:
     graph_file = argv[0];
     problem_file = argv[1];
     break;
  }

  uint64_t nproblems;
  uint64_t *problems;
  // Read the DIMACS problem specification file
  read_dimacs_spec(&problem_file, &nproblems, &problems);

  _sssp_args_t args = { .filename = graph_file,
                        .nproblems = nproblems,
                        .problems = problems
  };

  int e = hpx_init(&cfg);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  // register the actions
  _print_vertex_distance_index = HPX_REGISTER_ACTION(_print_vertex_distance_index_action);
  _print_vertex_distance       = HPX_REGISTER_ACTION(_print_vertex_distance_action);
  _main                        = HPX_REGISTER_ACTION(_main_action);

  return hpx_run(_main, &args, sizeof(args));
}
