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
          "\t-T, select a transport by number (see hpx_config.h)\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-l, set logging level\n"
          "\t-s, set stack size\n"
          "\t-p, set per-PE global heap size\n"
      "\t-r, set send/receive request limit\n"
          "\t-q, limit time for SSSP executions in seconds\n"
          "\t-a, instead resetting adj list between the runs, reallocate it\n"
          "\t-h, this help display\n");
}


static hpx_action_t _print_vertex_distance;
static int _print_vertex_distance_action(int *i)
{
  const hpx_addr_t target = hpx_thread_current_target();

  adj_list_vertex_t *vertex;
  if (!hpx_gas_try_pin(target, (void**)&vertex))
    return HPX_RESEND;

  printf("vertex: %d nbrs: %lu dist: %lu\n", *i, vertex->num_edges, vertex->distance);

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


static int _read_dimacs_spec(char **filename, uint64_t *nproblems, uint64_t **problems) {

  FILE *f = fopen(*filename, "r");
  assert(f);

  char line[LINE_MAX];
  int count = 0;
  while (fgets(line, sizeof(line), f) != NULL) {
    switch (line[0]) {
      case 'c': continue;
      case 's':
        sscanf(&line[1], " %lu", &((*problems)[count++]));
        break;
      case 'p':
        sscanf(&line[1], " aux sp ss %" PRIu64, nproblems);
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
  char *prob_file;
  uint64_t time_limit;
  int realloc_adj_list;
} _sssp_args_t;


static hpx_action_t _get_sssp_stat;
static int _get_sssp_stat_action(call_sssp_args_t* sargs)
{
  const hpx_addr_t target = hpx_thread_current_target();

  hpx_addr_t *sssp_stats;
  if (!hpx_gas_try_pin(target, (void**)&sssp_stats))
    return HPX_RESEND;

  sargs->sssp_stat = *sssp_stats;
  hpx_gas_unpin(target);

  return HPX_SUCCESS;
}

static hpx_action_t _print_sssp_stat;
static int _print_sssp_stat_action(_sssp_statistics *sssp_stat)
{
  const hpx_addr_t target = hpx_thread_current_target();
  _sssp_statistics *stat;
  if (!hpx_gas_try_pin(target, (void**)&stat))
    return HPX_RESEND;
  sssp_stat->useful_work  =  stat->useful_work;
  sssp_stat->useless_work =  stat->useless_work;
  sssp_stat->edge_traversal_count = stat->edge_traversal_count;
  hpx_thread_continue(sizeof(_sssp_statistics), sssp_stat);
  hpx_gas_unpin(target);

  return HPX_SUCCESS;
}

static hpx_action_t _main;
static int _main_action(_sssp_args_t *args) {
  const int realloc_adj_list = args->realloc_adj_list;

  // Create an edge list structure from the given filename
  edge_list_t el;
  printf("Allocating edge-list from file %s.\n", args->filename);
  hpx_call_sync(HPX_HERE, edge_list_from_file, &args->filename, sizeof(char*), &el, sizeof(el));
  printf("Edge List: #v = %lu, #e = %lu\n",
         el.num_vertices, el.num_edges);

  // Open the results file and write the basic info out
  FILE *results_file = fopen("sample.ss.chk", "a+");
  fprintf(results_file, "%s\n","p chk sp ss sssp");
  fprintf(results_file, "%s %s %s\n","f", args->filename,args->prob_file);
  fprintf(results_file, "%s %lu %lu %lu %lu\n","g", el.num_vertices, el.num_edges,el.min_edge_weight, el.max_edge_weight);

  call_sssp_args_t sargs;

  double total_elapsed_time = 0.0;

#ifdef GATHER_STAT
  printf("Gathering of statistics is enabled.\n");
  const hpx_addr_t sssp_stats = hpx_gas_global_calloc(1, sizeof(_sssp_statistics));
  sargs.sssp_stat = sssp_stats;

  uint64_t total_vertex_visit = 0;
  uint64_t total_edge_traversal = 0;
  uint64_t total_distance_updates = 0;
#endif // GATHER_STAT

  size_t *edge_traversed =(size_t *) calloc(args->nproblems, sizeof(size_t));
  double *elapsed_time = (double *) calloc(args->nproblems, sizeof(double));

  if(!realloc_adj_list) {
    // Construct the graph as an adjacency list
    hpx_call_sync(HPX_HERE, adj_list_from_edge_list, &el, sizeof(el), &sargs.graph, sizeof(sargs.graph));
  }

  for (int i = 0; i < args->nproblems; ++i) {
    if(total_elapsed_time > args->time_limit) {
      printf("Time limit of %" PRIu64 " seconds reached. Stopping further SSSP runs.\n", args->time_limit);
      args->nproblems = i;
      break;
    }

    if(realloc_adj_list) {
      // Construct the graph as an adjacency list
      hpx_call_sync(HPX_HERE, adj_list_from_edge_list, &el, sizeof(el), &sargs.graph, sizeof(sargs.graph));
    }

    sargs.source = args->problems[i];

    hpx_time_t now = hpx_time_now();

    // printf("Calling SSSP in the %d iteration\n", i);
    // Call the SSSP algorithm
    hpx_call_sync(HPX_HERE, call_sssp, &sargs, sizeof(sargs),NULL,0);

    double elapsed = hpx_time_elapsed_ms(now)/1e3;
    elapsed_time[i] = elapsed;
    total_elapsed_time += elapsed;

#ifdef GATHER_STAT
    _sssp_statistics *sssp_stat=(_sssp_statistics *)malloc(sizeof(_sssp_statistics));
    hpx_call_sync(sargs.sssp_stat, _print_sssp_stat,sssp_stat,sizeof(_sssp_statistics),sssp_stat,sizeof(_sssp_statistics));
     printf("\nuseful work = %lu,  useless work = %lu\n", sssp_stat->useful_work, sssp_stat->useless_work);

     total_vertex_visit += (sssp_stat->useful_work + sssp_stat->useless_work);
     total_distance_updates += sssp_stat->useful_work;
     total_edge_traversal += sssp_stat->edge_traversal_count;
#endif

#ifdef VERBOSE
    // Action to print the distances of each vertex from the source
    hpx_addr_t vertices = hpx_lco_and_new(el.num_vertices);
    for (int i = 0; i < el.num_vertices; ++i) {
      hpx_addr_t index = hpx_addr_add(sargs.graph, i * sizeof(hpx_addr_t), _index_array_block_size);
      hpx_call(index, _print_vertex_distance_index, &i, sizeof(i), vertices);
    }
    hpx_lco_wait(vertices);
    hpx_lco_delete(vertices, HPX_NULL);
#endif

    hpx_addr_t checksum_lco = HPX_NULL;
    hpx_call_sync(sargs.graph, dimacs_checksum, &el.num_vertices, sizeof(el.num_vertices),
                  &checksum_lco, sizeof(checksum_lco));
    size_t checksum = 0;
    hpx_lco_get(checksum_lco, sizeof(checksum), &checksum);
    hpx_lco_delete(checksum_lco, HPX_NULL);

    //printf("Computing GTEPS...\n");
    hpx_addr_t gteps_lco = HPX_NULL;
    hpx_call_sync(sargs.graph, gteps_calculate, &el.num_vertices, sizeof(el.num_vertices), &gteps_lco, sizeof(gteps_lco));
    size_t gteps = 0;
    hpx_lco_get(gteps_lco, sizeof(gteps), &gteps);
    hpx_lco_delete(gteps_lco, HPX_NULL);
    edge_traversed[i] = gteps;
    //printf("Gteps is %zu\n", gteps);

    printf("Finished problem %d in %.7f seconds (csum = %zu).\n", i, elapsed, checksum);
    fprintf(results_file, "d %zu\n", checksum);

    if(realloc_adj_list) {
      hpx_call_sync(sargs.graph, free_adj_list, NULL, 0, NULL, 0);
    } else {
      reset_adj_list(sargs.graph, &el);
    }
  }

  if(!realloc_adj_list) {
    hpx_call_sync(sargs.graph, free_adj_list, NULL, 0, NULL, 0);
  }

#ifdef GATHER_STAT
  double avg_time_per_source = total_elapsed_time/args->nproblems;
  double avg_vertex_visit  = total_vertex_visit/args->nproblems;
  double avg_edge_traversal = total_edge_traversal/args->nproblems;
  double avg_distance_updates = total_distance_updates/args->nproblems;

  printf("\navg_vertex_visit =  %f, avg_edge_traversal = %f, avg_distance_updates= %f\n", avg_vertex_visit, avg_edge_traversal, avg_distance_updates);

  FILE *fp;
  fp = fopen("perf.ss.res", "a+");

  fprintf(fp , "%s\n","p res sp ss sssp");
  fprintf(fp, "%s %s %s\n","f",args->filename,args->prob_file);
  fprintf(fp,"%s %lu %lu %lu %lu\n","g",el.num_vertices, el.num_edges,el.min_edge_weight, el.max_edge_weight);
  fprintf(fp,"%s %f\n","t",avg_time_per_source);
  fprintf(fp,"%s %f\n","v",avg_vertex_visit);
  fprintf(fp,"%s %f\n","e",avg_edge_traversal);
  fprintf(fp,"%s %f\n","i",avg_distance_updates);
  //fprintf(fp,"%s %s %f\n","c ", "The GTEPS measure is ",(total_edge_traversal/(total_elapsed_time*1.0E9)));

  fclose(fp);
#endif

#ifdef VERBOSE
  printf("\nElapsed time\n");
  for(int i = 0; i < args->nproblems; i++)
    printf("%f\n", elapsed_time[i]);

  printf("\nEdges traversed\n");
  for(int i = 0; i < args->nproblems; i++)
    printf("%zu\n", edge_traversed[i]);
#endif

  printf("\nTEPS statistics:\n");
  double *tm = (double*)malloc(sizeof(double)*args->nproblems);
  double *stats = (double*)malloc(sizeof(double)*9);

  for(int i = 0; i < args->nproblems; i++)
    tm[i] = edge_traversed[i]/elapsed_time[i];

  statistics (stats, tm, args->nproblems);
  PRINT_STATS("TEPS", 1);

  hpx_shutdown(HPX_SUCCESS);
  return HPX_SUCCESS;
}


int main(int argc, char *const argv[argc]) {
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;
  uint64_t time_limit = 1000;
  int realloc_adj_list = 0;

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:T:d:Dl:s:p:r:q:ah")) != -1) {
    switch (opt) {
    case 'c':
      cfg.cores = atoi(optarg);
      break;
    case 't':
      cfg.threads = atoi(optarg);
      break;
    case 'T':
      cfg.transport = atoi(optarg);
      assert(0 <= cfg.transport && cfg.transport < HPX_TRANSPORT_MAX);
      break;
    case 'D':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = HPX_LOCALITY_ALL;
      break;
    case 'd':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = atoi(optarg);
      break;
    case 'l':
      cfg.log_level = atoi(optarg);
      break;
    case 's':
      cfg.stack_bytes = strtoul(optarg, NULL, 0);
      break;
    case 'p':
      cfg.heap_bytes = strtoul(optarg, NULL, 0);
      break;
    case 'r':
      cfg.req_limit = strtoul(optarg, NULL, 0);
      break;
    case 'q':
      time_limit = strtoul(optarg, NULL, 0);
      break;
    case 'a':
      realloc_adj_list = 1;
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
  _read_dimacs_spec(&problem_file, &nproblems, &problems);

  _sssp_args_t args = { .filename = graph_file,
                        .nproblems = nproblems,
                        .problems = problems,
                        .prob_file = problem_file,
            .time_limit = time_limit,
            .realloc_adj_list = realloc_adj_list
  };

  int e = hpx_init(&cfg);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  // register the actions
  _print_vertex_distance_index = HPX_REGISTER_ACTION(_print_vertex_distance_index_action);
  _print_vertex_distance       = HPX_REGISTER_ACTION(_print_vertex_distance_action);
  _get_sssp_stat               = HPX_REGISTER_ACTION(_get_sssp_stat_action);
  _print_sssp_stat             = HPX_REGISTER_ACTION(_print_sssp_stat_action);
  _main                        = HPX_REGISTER_ACTION(_main_action);

  return hpx_run(_main, &args, sizeof(args));
}
