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

/// @file
/// A simple fibonacci number computation to demonstrate HPX.
/// This example calculates a fibonacci number using recursion, where each
/// level of recursion is executed by a different HPX thread. (Of course, this
/// is not an efficient way to calculate a fibonacci number but it does
/// demonstrate some of the basic of HPX and it may demonstrate a
/// <em>pattern of computation</em> that might be used in the real world.)

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "hpx/hpx.h"
#include "common.h"

static void usage(FILE *stream) {
  fprintf(stream, "Usage: fibonacci [options] NUMBER\n"
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

static hpx_action_t _fib      = 0;
static hpx_action_t _fib_main = 0;

static int _fib_action(int *args) {
  int n = *args;

  if (n < 2)
    HPX_THREAD_CONTINUE(n);

  hpx_addr_t peers[] = {
    HPX_HERE,
    HPX_HERE
  };

  int ns[] = {
    n - 1,
    n - 2
  };

  hpx_addr_t futures[] = {
    hpx_lco_future_new(sizeof(int)),
    hpx_lco_future_new(sizeof(int))
  };

  int fns[] = {
    0,
    0
  };

  void *addrs[] = {
    &fns[0],
    &fns[1]
  };

  int sizes[] = {
    sizeof(int),
    sizeof(int)
  };

  hpx_call(peers[0], _fib, &ns[0], sizeof(int), futures[0]);
  hpx_call(peers[1], _fib, &ns[1], sizeof(int), futures[1]);
  hpx_lco_get_all(2, futures, sizes, addrs, NULL);
  hpx_lco_delete(futures[0], HPX_NULL);
  hpx_lco_delete(futures[1], HPX_NULL);

  int fn = fns[0] + fns[1];
  HPX_THREAD_CONTINUE(fn);
  return HPX_SUCCESS;
}

static int _fib_main_action(int *args) {
  int n = *args;
  int fn = 0;                                   // fib result
  fprintf(test_log, "fib(%d)=", n); fflush(stdout);
  hpx_time_t now = hpx_time_now();
  hpx_call_sync(HPX_HERE, _fib, &n, sizeof(n), &fn, sizeof(fn));
  double elapsed = hpx_time_elapsed_ms(now)/1e3;

  fprintf(test_log, "%d\n", fn);
  fprintf(test_log, "seconds: %.7f\n", elapsed);
  fprintf(test_log, "localities: %d\n", HPX_LOCALITIES);
  fprintf(test_log, "threads/locality: %d\n\n", HPX_THREADS);
  fclose(test_log);
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;

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

  int e = hpx_init(&cfg);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  test_log = fopen("test.log", "a+");

  // register the fib action
  _fib      = HPX_REGISTER_ACTION(_fib_action);
  _fib_main = HPX_REGISTER_ACTION(_fib_main_action);

  // run the main action
  return hpx_run(_fib_main, &n, sizeof(n));
}

