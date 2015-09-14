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
//

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <hpx/hpx.h>

#ifdef HAVE_MPI
# include <mpi.h>
#endif

/// This is a microbenchmark to evaluate the performance of collective LCO operations in HPX.
///
/// The included micro-benchmarks are:
/// 1. allreduce

// Global send/receive buffers
unsigned char *sbuf;
unsigned char *rbuf;

/// Allreduce "reduction" operations.
static void _init_handler(unsigned char *id, const size_t size) {
  for (int i = 0; i < size; ++i) {
    id[i] = rand();
  }
}
static HPX_ACTION(HPX_FUNCTION, 0, _init, _init_handler);

static void
_min_handler(unsigned char *out, const unsigned char *in, const size_t size) {
  for (int i = 0; i < size; ++i) {
    if (out[i] > in[i]) *out = *in;
  }
}
static HPX_ACTION(HPX_FUNCTION, 0, _min, _min_handler);

/// Use a set-get pair for the allreduce operation.
static int
_allreduce_set_get_handler(hpx_addr_t allreduce, size_t size) {
  hpx_lco_set_lsync(allreduce, size, sbuf, HPX_NULL);
  hpx_lco_get(allreduce, size, rbuf);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _allreduce_set_get,
                  _allreduce_set_get_handler, HPX_ADDR, HPX_SIZE_T);

/// Use a synchronous join for the allreduce operation.
static int
_allreduce_join_handler(hpx_addr_t allreduce, size_t size) {
  hpx_addr_t f = hpx_lco_future_new(0);
  int id = (HPX_LOCALITY_ID * HPX_THREADS) + HPX_THREAD_ID;
  hpx_lco_allreduce_join_async(allreduce, id, size, sbuf, rbuf, f);
  hpx_lco_wait(f);
  hpx_lco_delete(f, HPX_NULL);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _allreduce_join, _allreduce_join_handler,
                  HPX_ADDR, HPX_SIZE_T);

/// Use join-sync for the allreduce operation.
static int
_allreduce_join_sync_handler(hpx_addr_t allreduce, size_t size) {
  int id = (HPX_LOCALITY_ID * HPX_THREADS) + HPX_THREAD_ID;
  hpx_lco_allreduce_join_sync(allreduce, id, size, sbuf, rbuf);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _allreduce_join_sync,
                  _allreduce_join_sync_handler, HPX_ADDR, HPX_SIZE_T);

#ifdef HAVE_MPI
static int
_allreduce_mpi_handler(hpx_addr_t allreduce, size_t size) {
  MPI_Allreduce(sbuf, rbuf, size, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _allreduce_mpi,
                  _allreduce_mpi_handler, HPX_ADDR, HPX_SIZE_T);
#endif

static int
_fill_node_handler(hpx_action_t op, hpx_addr_t allreduce, size_t size) {
  for (int i = 0; i < HPX_THREADS; ++i) {
    hpx_xcall(HPX_HERE, op, HPX_NULL, allreduce, size);
  }
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _fill_node,
                  _fill_node_handler, HPX_ACTION_T, HPX_ADDR, HPX_SIZE_T);

/// A utility that tests a certain leaf function through I iterations.
static int _benchmark(char *name, hpx_action_t op, int iters, size_t size) {
  int ranks = HPX_LOCALITIES * HPX_THREADS;
  hpx_addr_t allreduce = hpx_lco_allreduce_new(ranks, ranks, size,
                                               _init, _min);
  hpx_time_t start = hpx_time_now();
  for (int i = 0; i < iters; ++i) {
    hpx_bcast(_fill_node, HPX_NULL, HPX_NULL, &op, &allreduce, &size);
  }

  double elapsed = hpx_time_elapsed_ms(start);
  printf("%s: %.7f\n", name, elapsed/iters);
  hpx_lco_delete(allreduce, HPX_NULL);
  return HPX_SUCCESS;
}
#define _XSTR(s) _STR(s)
#define _STR(l) #l
#define _BENCHMARK(op, iters, size) _benchmark(_XSTR(op), op, iters, size)

static HPX_ACTION_DECL(_main);
static int _main_action(int iters, size_t size) {
  printf("collbench(iters=%d, size=%lu)\n", iters, size);
  printf("time resolution: milliseconds\n");
  fflush(stdout);

#ifdef HAVE_MPI
  _BENCHMARK(_allreduce_mpi, iters, size);
#endif

  _BENCHMARK(_allreduce_set_get, iters, size);
  _BENCHMARK(_allreduce_join, iters, size);
  _BENCHMARK(_allreduce_join_sync, iters, size);

  free(sbuf);
  free(rbuf);
  hpx_exit(HPX_SUCCESS);
}  
static HPX_ACTION(HPX_DEFAULT, 0, _main, _main_action, HPX_INT, HPX_SIZE_T);

static void _usage(FILE *f, int error) {
  fprintf(f, "Usage: collbench -i iters -s size\n"
             "\t -i iters: number of iterations\n"
             "\t -s  size: size of the buffer to use for the collective\n"
             "\t -h      : show help\n");
  hpx_print_help();
  fflush(f);
  exit(error);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  int iters = 100;
  size_t size = 8;
  int opt = 0;
  while ((opt = getopt(argc, argv, "i:s:h?")) != -1) {
    switch (opt) {
     case 'i':
       iters = atoi(optarg);
       break;
     case 's':
       size = atoi(optarg);
       break;
     case 'h':
       _usage(stdout, EXIT_SUCCESS);
     default:
       _usage(stderr, EXIT_FAILURE);
    }
  }

  argc -= optind;
  argv += optind;

  sbuf = calloc(1, size);
  rbuf = calloc(1, size);

  e = hpx_run(&_main, &iters, &size);
  assert(e == HPX_SUCCESS);
  hpx_finalize();
}
