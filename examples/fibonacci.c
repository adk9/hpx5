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


static void _usage(FILE *stream) {
  fprintf(stream, "Usage: fibonacci [hpx-options] NUMBER\n");
  hpx_print_help();
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
  printf("fib(%d)=", n); fflush(stdout);
  hpx_time_t now = hpx_time_now();
  hpx_call_sync(HPX_HERE, _fib, &n, sizeof(n), &fn, sizeof(fn));
  double elapsed = hpx_time_elapsed_ms(now)/1e3;

  printf("%d\n", fn);
  printf("seconds: %.7f\n", elapsed);
  printf("localities: %d\n", HPX_LOCALITIES);
  printf("threads/locality: %d\n", HPX_THREADS);
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int n = 0;
  
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

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

  // register the fib action
  _fib      = HPX_REGISTER_ACTION(_fib_action);
  _fib_main = HPX_REGISTER_ACTION(_fib_main_action);

  // run the main action
  return hpx_run(_fib_main, &n, sizeof(n));
}

