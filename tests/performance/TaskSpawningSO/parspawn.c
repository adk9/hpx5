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
#include <inttypes.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "common.h"

/// ----------------------------------------------------------------------------
/// @file examples/hpx/parspawn.c
/// This file implements a parallel (tree) spawn, that uses HPX
/// threads to spawn many NOP operations in parallel.
/// ----------------------------------------------------------------------------

static void usage(FILE *stream) {
  fprintf(stream, "Usage: parspawn [options] NUMBER\n"
          "\t-c, number of cores to run on\n"
          "\t-t, number of scheduler threads\n"
          "\t-T, select a transport by number (see hpx_config.h)\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-l, set logging level\n"
          "\t-s, set stack size\n"
          "\t-p, set per-PE global heap size\n"
          "\t-r, set send/receive request limit\n"
          "\t-h, this help display\n");
}

static hpx_action_t _nop     = 0;
static hpx_action_t _main    = 0;


// The empty action
static int _nop_action(void *args) {
  hpx_thread_exit(HPX_SUCCESS);
}

static int _main_action(int *args) {
  int n = *args;
  fprintf(test_log, "parspawn(%d)\n", n); fflush(stdout);

  hpx_time_t now = hpx_time_now();
  hpx_par_call_sync(_nop, 0, n, 8, 1000, 0, NULL, 0, 0);
  double elapsed = hpx_time_elapsed_ms(now)/1e3;

  fprintf(test_log, "seconds: %.7f\n", elapsed);
  fprintf(test_log, "localities:   %d\n", HPX_LOCALITIES);
  fprintf(test_log, "threads:      %d\n\n", HPX_THREADS);

  fclose(test_log);
  hpx_shutdown(HPX_SUCCESS);
}

/// ----------------------------------------------------------------------------
/// The main function parses the command line, sets up the HPX runtime system,
/// and initiates the first HPX thread to perform parspawn(n).
///
/// @param argc    - number of strings
/// @param argv[0] - parspawn
/// @param argv[1] - number of cores to use, '0' means use all
/// @param argv[2] - n
/// ----------------------------------------------------------------------------
int
main(int argc, char *argv[])
{
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;
  //cfg.gas          = HPX_GAS_SMP;

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:T:d:Dl:s:p:r:q:h")) != -1) {
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
     case 'h':
      usage(stdout);
      return 0;
     case '?':
     default:
      usage(stderr);
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
    usage(stderr);
    return -1;
   case 1:
     n = atoi(argv[0]);
     break;
  }

  if (hpx_init(&cfg)) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return 1;
  }

  test_log = fopen("test.log", "a+");

  // register the actions
  _nop     = HPX_REGISTER_ACTION(_nop_action);
  _main    = HPX_REGISTER_ACTION(_main_action);

  // run the main action
  return hpx_run(_main, &n, sizeof(n));
}
