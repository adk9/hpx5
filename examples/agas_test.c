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

static int get_rank_action(void *args) {
  int rank = HPX_LOCALITY_ID;
  HPX_THREAD_CONTINUE(rank);
}

static int root_action(void *args) {
  printf("root locality: %d, thread: %d.\n", HPX_LOCALITY_ID, HPX_THREAD_ID);
  hpx_addr_t base = hpx_lco_future_array_new(2, sizeof(int), 1);
  hpx_addr_t other = hpx_lco_future_array_at(base, 1, sizeof(int));

  int r = 0;
  hpx_call_sync(other, get_rank, NULL, 0, &r, sizeof(r));
  printf("target locality's ID (before move): %d\n", r);

  if (r == HPX_LOCALITY_ID) {
    printf("AGAS test: failed.\n");
    hpx_shutdown(0);
  }

  hpx_addr_t done = hpx_lco_future_new(0);
  // move address to our locality.
  printf("initiating AGAS move from (%lu,%u,%u) to (%lu,%u,%u).\n",
         other.offset, other.base_id, other.block_bytes,
         HPX_HERE.offset, HPX_HERE.base_id, HPX_HERE.block_bytes);
  hpx_gas_move(other, HPX_HERE, done);
  if (hpx_lco_wait(done) != HPX_SUCCESS)
    printf("error in hpx_move().\n");

  hpx_lco_delete(done, HPX_NULL);

  hpx_call_sync(other, get_rank, NULL, 0, &r, sizeof(r));
  printf("target locality's rank (after move): %d\n", r);

  printf("AGAS test: %s.\n", ((r == hpx_get_my_rank()) ? "passed" : "failed"));
  hpx_shutdown(HPX_SUCCESS);
}

static void usage(FILE *f) {
  fprintf(f, "Usage: agas_test [options] ROUNDS \n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, show help\n");
}

int main(int argc, char *argv[argc]) {
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;
  cfg.gas          = HPX_GAS_AGAS;

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
      usage(stdout);
      return 0;
     case '?':
     default:
      usage(stderr);
      return -1;
    }
  }

  if (hpx_init(&cfg)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  int ranks = HPX_LOCALITIES;
  if (ranks < 2) {
    fprintf(stderr, "A minimum of 2 localities are required to run this test.");
    return -1;
  }

  root     = HPX_REGISTER_ACTION(root_action);
  get_rank = HPX_REGISTER_ACTION(get_rank_action);
  return hpx_run(root, NULL, 0);
}
