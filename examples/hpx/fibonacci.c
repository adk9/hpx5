/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Pingong example
  examples/hpx/pingpong.c

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "hpx/hpx.h"

#include "debug.h"

static void _usage(FILE *stream) {
  fprintf(stream, "Usage: fibonaccihpx [options] NUMBER\n"
          "\t-c, number of cores to run on\n"
          "\t-t, number of scheduler threads\n"
          "\t-d, wait for debugger\n"
          "\t-h, this help display\n");
}

static hpx_action_t _fib = 0;
static hpx_action_t _fib_main = 0;

static int _fib_action(void *args) {
  int n = *(int*)args;

  if (n < 2)
    hpx_thread_continue(sizeof(n), &n);

  // int rank = hpx_get_my_rank();
  // int ranks = hpx_get_num_ranks();

  hpx_addr_t peers[] = {
    HPX_HERE, // hpx_addr_from_rank((rank + ranks - 1) % ranks),
    HPX_HERE // hpx_addr_from_rank((rank + 1) % ranks)
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
  hpx_lco_get_all(2, futures, addrs, sizes);
  hpx_lco_delete(futures[0], HPX_NULL);
  hpx_lco_delete(futures[1], HPX_NULL);

  int fn = fns[0] + fns[1];
  hpx_thread_continue(sizeof(fn), &fn);
  return HPX_SUCCESS;
}

static int _fib_main_action(void *args) {
  int n = *(int*)args;
  int fn = 0;                                   // fib result
  printf("fib(%d)=", n); fflush(stdout);
  hpx_time_t clock = hpx_time_now();
  hpx_addr_t future = hpx_lco_future_new(sizeof(int));
  hpx_call(HPX_HERE, _fib, &n, sizeof(n), future);
  hpx_lco_get(future, &fn, sizeof(fn));
  hpx_lco_delete(future, HPX_NULL);

  double time = hpx_time_elapsed_ms(clock)/1e3;

  printf("%d\n", fn);
  printf("seconds: %.7f\n", time);
  printf("localities: %d\n", hpx_get_num_ranks());
  printf("threads/locality: %d\n", hpx_get_num_threads());
  hpx_shutdown(0);
}

int main(int argc, char *argv[]) {
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
  _fib = hpx_register_action("_fib", _fib_action);
  _fib_main = hpx_register_action("_fib_main", _fib_main_action);

  // run the main action
  return hpx_run(_fib_main, &n, sizeof(n));
}
