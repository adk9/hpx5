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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <hpx/hpx.h>

/// Allreduce "reduction" operations.
static void initdouble_handler(double *input, const size_t size) {
  *input = 99999999.0;
}
static HPX_ACTION(HPX_FUNCTION, 0, initdouble, initdouble_handler);

static void mindouble_handler(double *output, const double *input, const size_t size) {
  if ( *output > *input ) *output = *input;
}
static HPX_ACTION(HPX_FUNCTION, 0, mindouble, mindouble_handler);


/// Use a set-get pair for the allreduce operation.
static int
_allreduce_set_get_handler(hpx_addr_t allreduce, size_t size) {
  char buf[size];
  hpx_lco_set_lsync(allreduce, size, buf, HPX_NULL);
  hpx_lco_get(allreduce, size, buf);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _allreduce_set_get,
                  _allreduce_set_get_handler, HPX_ADDR, HPX_SIZE_T);

/// Use a synchronous join for the allreduce operation.
static int
_allreduce_join_handler(hpx_addr_t allreduce, size_t size) {
  char buf[size];
  hpx_addr_t f = hpx_lco_future_new(0);
  hpx_lco_allreduce_join_async(allreduce, HPX_LOCALITY_ID, size, buf, buf, f);
  hpx_lco_wait(f);
  hpx_lco_delete(f, HPX_NULL);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _allreduce_join, _allreduce_join_handler,
                  HPX_ADDR, HPX_SIZE_T);

/// Use join-sync for the allreduce operation.
static int
_allreduce_join_sync_handler(hpx_addr_t allreduce, size_t size) {
  char buf[size];
  hpx_lco_allreduce_join_sync(allreduce, HPX_LOCALITY_ID, size, buf, buf);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _allreduce_join_sync,
                  _allreduce_join_sync_handler, HPX_ADDR, HPX_SIZE_T);


/// A utility that tests a certain leaf function through I iterations.
static int _benchmark(char *name, hpx_action_t op, int iters, size_t size) {
  int ranks = HPX_LOCALITIES;

  hpx_time_t start = hpx_time_now();
  hpx_addr_t allreduce = hpx_lco_allreduce_new(ranks, ranks, size,
                                               initdouble, mindouble);
  for (int i = 0; i < iters; ++i) {
    hpx_bcast_rsync(op, &allreduce, &size);
  }
  hpx_lco_delete(allreduce, HPX_NULL);

  double elapsed = hpx_time_elapsed_ms(start);
  printf("%s: %.7f\n", name, elapsed/iters);
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

  _BENCHMARK(_allreduce_set_get, iters, size);
  _BENCHMARK(_allreduce_join, iters, size);
  _BENCHMARK(_allreduce_join_sync, iters, size);

  hpx_shutdown(HPX_SUCCESS);
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

  return hpx_run(&_main, &iters, &size);
}
