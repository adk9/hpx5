// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hpx/hpx.h>


static void _usage(FILE *f, int error) {
  fprintf(f, "Usage: gasbench -i iters -s size\n"
             "\t -i iters: number of iterations\n"
             "\t -s size: size of GAS objects to allocate\n"
             "\t -h      : show help\n");
  hpx_print_help();
  fflush(f);
  exit(error);
}

typedef struct _par_alloc_args {
  int iters;
  int size;
} _par_alloc_args_t;

int _par_alloc(const int tid, void *a) {
  _par_alloc_args_t *args = (_par_alloc_args_t*)a;
  int iters = args->iters;
  size_t size = args->size;
  hpx_time_t start = hpx_time_now();
  for (int i = 0; i < iters; ++i) {
    hpx_addr_t addr = hpx_gas_alloc_local(1, size, 0);
    hpx_gas_free(addr, HPX_NULL);
  }
  double elapsed = hpx_time_elapsed_us(start);
  printf("%d: alloc+free: %.7f\n", tid, elapsed/iters);

  start = hpx_time_now();
  for (int i = 0; i < iters; ++i) {
    hpx_addr_t addr = hpx_gas_calloc_local(1, size, 0);
    hpx_gas_free(addr, HPX_NULL);
  }
  elapsed = hpx_time_elapsed_us(start);
  printf("%d: calloc+free: %.7f\n", tid, elapsed/iters);

  hpx_addr_t *addrs = malloc(sizeof(*addrs)*iters);
  assert(addrs);

  start = hpx_time_now();
  for (int i = 0; i < iters; ++i) {
    addrs[i] = hpx_gas_alloc_local(1, size, 0);
  }

  for (int i = 0; i < iters; ++i) {
    hpx_gas_free(addrs[i], HPX_NULL);
  }
  elapsed = hpx_time_elapsed_us(start);
  printf("%d: allocs-then-frees: %.7f\n", tid, elapsed/iters);

  start = hpx_time_now();
  for (int i = 0; i < iters; ++i) {
    addrs[i] = hpx_gas_alloc_local(1, size, 0);
  }

  for (int i = 0; i < iters; ++i) {
    hpx_gas_free(addrs[i], HPX_NULL);
  }
  elapsed = hpx_time_elapsed_us(start);
  printf("%d: callocs-then-frees: %.7f\n", tid, elapsed/iters);
  return HPX_SUCCESS;
}

static int _main_action(int iters, size_t size) {
  printf("gasbench(iters=%d, size=%lu, threads=%d)\n",
         iters, size, HPX_THREADS);
  printf("time resolution: microseconds\n");
  fflush(stdout);

  _par_alloc_args_t args = { .iters = iters, .size = size };
  hpx_par_for_sync(_par_alloc, 0, HPX_THREADS, &args);

  hpx_exit(HPX_SUCCESS);
}
static HPX_ACTION(HPX_DEFAULT, 0, _main, _main_action, HPX_INT, HPX_SIZE_T);

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  int iters = 5;
  size_t size = 8192;
  int opt = 0;
    while ((opt = getopt(argc, argv, "i:s:h?")) != -1) {
    switch (opt) {
     case 'i':
       iters = atoi(optarg);
       break;
     case 's':
       size = atol(optarg);
       break;
     case 'h':
       _usage(stdout, EXIT_SUCCESS);
     default:
       _usage(stderr, EXIT_FAILURE);
    }
  }

  argc -= optind;
  argv += optind;

  e = hpx_run(&_main, &iters, &size);
  hpx_finalize();
  return e;
}
