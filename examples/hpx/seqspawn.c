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
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, this help display\n");
}

static hpx_action_t nop = 0;
static hpx_action_t seq_main = 0;

static hpx_addr_t and;

// The empty action
static int
nop_action(void *args)
{
  hpx_lco_and_set(and, HPX_NULL);
  return HPX_SUCCESS;
}

static int
seq_main_action(void *args) {
  int n = *(int*)args;
  hpx_addr_t addr = HPX_HERE;
  printf("seqspawn(%d)\n", n); fflush(stdout);

  hpx_time_t clock = hpx_time_now();
  and = hpx_lco_and_new(n);
  for (int i = 0; i < n; i++)
    hpx_call(addr, nop, 0, 0, HPX_NULL);
  hpx_lco_wait(and);
  double time = hpx_time_elapsed_ms(clock)/1e3;

  printf("seconds: %.7f\n", time);
  printf("localities:   %d\n", hpx_get_num_ranks());
  printf("threads:      %d\n", hpx_get_num_threads());
  hpx_lco_delete(and, HPX_NULL);
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

  int debug = NO_RANKS;
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
      debug = ALL_RANKS;
      break;
     case 'd':
      debug = atoi(optarg);
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

  wait_for_debugger(debug);

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
