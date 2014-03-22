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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include "hpx/hpx.h"

#include "debug.h"

/// ----------------------------------------------------------------------------
/// @file examples/hpx/seqspawn.c
/// This file implements a sequential task spawn microbenchmark. A
/// series of NOP threads are spawned sequentially.
/// ----------------------------------------------------------------------------

static void _usage(FILE *stream) {
  fprintf(stream, "Usage: seqspawn [options] NUMBER\n"
          "\t-c, number of cores to run on\n"
          "\t-t, number of scheduler threads\n"
          "\t-d, wait for debugger\n"
          "\t-h, this help display\n");
}

static hpx_action_t nop = 0;
static hpx_action_t seq_main = 0;

static hpx_addr_t counter;

// The empty action
static int
nop_action(void *args)
{
  hpx_lco_counter_incr(counter, 1);
  return HPX_SUCCESS;
}

static int
seq_main_action(void *args) {
  int n = *(int*)args;
  hpx_addr_t addr = hpx_addr_from_rank(hpx_get_my_rank());
  printf("seqspawn(%d)\n", n); fflush(stdout);

  hpx_time_t clock = hpx_time_now();
  counter = hpx_lco_counter_new(n);
  for (int i = 0; i < n; i++)
    hpx_call(addr, nop, 0, 0, HPX_NULL);
  hpx_lco_counter_wait(counter);
  double time = hpx_time_elapsed_ms(clock)/1e3;

  printf("seconds: %.7f\n", time);
  printf("localities:   %d\n", hpx_get_num_ranks());
  printf("threads:      %d\n", hpx_get_num_threads());
  hpx_lco_counter_delete(counter);
  hpx_shutdown(0);
  return HPX_SUCCESS;
}

/// ----------------------------------------------------------------------------
/// The main function parses the command line, sets up the HPX runtime system,
/// and initiates the first HPX thread to perform seqspawn(n).
///
/// @param argc    - number of strings
/// @param argv[0] - seqspawn
/// @param argv[1] - number of cores to use, '0' means use all
/// @param argv[2] - n
/// ----------------------------------------------------------------------------
int
main(int argc, char *argv[])
{
  hpx_config_t cfg = {
    .cores = 0,
    .threads = 0,
    .stack_bytes = 0
  };

  bool debug = false;
  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:dh")) != -1) {
    switch (opt) {
     case 'c':
      cfg.cores = atoi(optarg);
      break;
     case 't':
      cfg.threads = atoi(optarg);
      break;
     case 'd':
      debug = true;
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

  int n = 0;
  switch (argc) {
   case 0:
    fprintf(stderr, "\nMissing fib number.\n"); // fall through
   default:
    _usage(stderr);
    return -1;
   case 1:
     n = atoi(argv[0]);
     break;
  }

  if (debug)
    wait_for_debugger();

  int e = hpx_init(&cfg);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  // register the fib action
  nop = hpx_register_action("nop", nop_action);
  seq_main = hpx_register_action("seq_main", seq_main_action);

  // run the main action
  return hpx_run(seq_main, &n, sizeof(n));
}
