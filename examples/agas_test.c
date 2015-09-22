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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"

/// ----------------------------------------------------------------------------
/// @file examples/hpx/agas_test.c
///
/// This file implements a simple AGAS test. A thread on the root
/// locality allocates two futures with a cyclic distribution, one on
/// the root locality and the other on a remote locality. It invokes a
/// "get-rank" action on the remote future, initiates a synchronous
/// move of the remote future address to the root locality, and
/// re-executes the "get-rank" action on the address.
/// ----------------------------------------------------------------------------

static hpx_action_t root = 0;
static hpx_action_t get_rank = 0;

static int get_rank_action(void *args, size_t size) {
  int rank = HPX_LOCALITY_ID;
  HPX_THREAD_CONTINUE(rank);
}

static int root_action(void *args, size_t size) {
  printf("root locality: %d, thread: %d.\n", HPX_LOCALITY_ID, HPX_THREAD_ID);
  hpx_addr_t base = hpx_lco_future_array_new(2, sizeof(int), 1);
  hpx_addr_t other = hpx_lco_future_array_at(base, 1, sizeof(int), 1);

  int r = 0;
  hpx_call_sync(other, get_rank, &r, sizeof(r), NULL, 0);
  printf("target locality's ID (before move): %d\n", r);

  if (r == HPX_LOCALITY_ID) {
    printf("AGAS test: failed.\n");
    hpx_exit(0);
  }

  hpx_addr_t done = hpx_lco_future_new(0);
  // move address to our locality.
  printf("initiating AGAS move from (%lu) to (%lu).\n", other, HPX_HERE);
  hpx_gas_move(other, HPX_HERE, done);
  if (hpx_lco_wait(done) != HPX_SUCCESS)
    printf("error in hpx_move().\n");

  hpx_lco_delete(done, HPX_NULL);

  hpx_call_sync(other, get_rank, &r, sizeof(r), NULL, 0);
  printf("target locality's rank (after move): %d\n", r);

  printf("AGAS test: %s.\n", ((r == hpx_get_my_rank()) ? "passed" : "failed"));
  hpx_exit(HPX_SUCCESS);
}

static void usage(FILE *f) {
  fprintf(f, "Usage: agas_test [options] ROUNDS \n"
          "\t-h, show help\n");
  hpx_print_help();
  fflush(f);
}

int main(int argc, char *argv[argc]) {

  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }
  
  int opt = 0;
  while ((opt = getopt(argc, argv, "h?")) != -1) {
    switch (opt) {
     case 'h':
      usage(stdout);
      return 0;
     case '?':
     default:
      usage(stderr);
      return -1;
    }
  }

  int ranks = HPX_LOCALITIES;
  if (ranks < 2) {
    fprintf(stderr, "A minimum of 2 localities are required to run this test.");
    return -1;
  }

  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, root, root_action, HPX_POINTER, HPX_SIZE_T);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, get_rank, get_rank_action, HPX_POINTER, HPX_SIZE_T);
  int e = hpx_run(&root, NULL, 0);
  hpx_finalize();
  return e;
}
