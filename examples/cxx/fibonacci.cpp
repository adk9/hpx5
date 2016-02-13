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

/// @file
/// A simple fibonacci number computation to demonstrate HPX.
/// This example calculates a fibonacci number using recursion, where each
/// level of recursion is executed by a different HPX thread. (Of course, this
/// is not an efficient way to calculate a fibonacci number but it does
/// demonstrate some of the basic of HPX and it may demonstrate a
/// <em>pattern of computation</em> that might be used in the real world.)

#include <iostream>

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "hpx/hpx++.h"

static void _usage(std::ostream& os, int error) {
  os << "Usage: fibonacci [options] NUMBER\n" << "\t-h, show help\n";
  hpx_print_help();
  exit(error);
}

// static hpx_action_t _fib      = 0;
// static hpx_action_t _fib_main = 0;

int _fib_action(int n);
hpx::Action<HPX_DEFAULT, HPX_ATTR_NONE, decltype(_fib_action), int> _fib;

int _fib_action(int n) {
  
  if (n < 2) {
    return HPX_THREAD_CONTINUE(n);
  }

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

  size_t sizes[] = {
    sizeof(int),
    sizeof(int)
  };
  
  _fib.call(peers[0], futures[0], ns[0]);
  _fib.call(peers[1], futures[1], ns[1]);
  
  hpx_lco_get_all(2, futures, sizes, addrs, NULL);
  hpx_lco_delete(futures[0], HPX_NULL);
  hpx_lco_delete(futures[1], HPX_NULL);

  int fn = fns[0] + fns[1];
  return HPX_THREAD_CONTINUE(fn);
}

static int _fib_main_action(int n) {
  int fn = 0;                                   // fib result
  std::cout << "fib("<< n << ")=";
  hpx_time_t now = hpx_time_now();

  _fib.call_sync(HPX_HERE, fn, n);
  double elapsed = hpx_time_elapsed_ms(now)/1e3;

  std::cout << fn << std::endl;
  std::cout << "seconds: " << elapsed << std::endl;
  std::cout << "localities: " << HPX_LOCALITIES << std::endl;
  std::cout << "threads/locality: " << HPX_THREADS << std::endl;
  hpx_exit(HPX_SUCCESS);
}
auto _fib_main = hpx::make_action(_fib_main_action);

int main(int argc, char *argv[]) {
  
  _fib._register(_fib_action);
  
  int e = hpx::init(&argc, &argv);
  if (e) {
    std::cerr << "HPX: failed to initialize.\n";
    return e;
  }

  // parse the command line
  int opt = 0;
  while ((opt = getopt(argc, argv, "h?")) != -1) {
    switch (opt) {
     case 'h':
       _usage(std::cout, EXIT_SUCCESS);
     case '?':
     default:
       _usage(std::cerr, EXIT_FAILURE);
    }
  }

  argc -= optind;
  argv += optind;

  int n = 0;
  switch (argc) {
   case 0:
     std::cerr << "\nMissing fib number.\n"; // fall through
   default:
     _usage(std::cerr, EXIT_FAILURE);
   case 1:
     n = atoi(argv[0]);
     break;
  }

  // run the main action
  _fib_main.run(n);
  
  hpx::finalize();
  
  return e;
}

