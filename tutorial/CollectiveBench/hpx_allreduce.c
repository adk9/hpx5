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

#define BENCHMARK "HPX AllReduce Latency Benchmark"
/*
 * Copyright (C) 2002-2014 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University.
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level OMB directory.
 */

#include <stdio.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include "common.h"

# define HEADER "# " BENCHMARK "\n"

static hpx_action_t _main = 0;
static hpx_action_t _init = 0;
static hpx_action_t _reduce = 0;

static int _reduce_action(const InitArgs *args) {
  int size, skip, full = 1;
  double latency;
  double maxVal, max_time, min_time, avg_time;
  int64_t t_start = 0, t_stop = 0, timer = 0;

  int THREADS = HPX_LOCALITIES;
  int MYTHREAD = HPX_LOCALITY_ID;

  hpx_addr_t target = hpx_thread_current_target();
  Domain *ld;
  if (!hpx_gas_try_pin(target, (void**)&ld))
    return HPX_RESEND;

  int max_msg_size = ld->maxSize;

  for (size = 1; size <= max_msg_size; size *= 2) {
    if (size > LARGE_MESSAGE_SIZE) {
      skip = SKIP_LARGE;
      iterations = iterations_large;
    }
    else {
      skip = SKIP;
    }

    timer = 0;
    for (int i = 0; i < iterations + skip ; i++) {
      t_start = TIME();

      hpx_lco_set(ld->collVal, sizeof(double), &size, HPX_NULL, HPX_NULL);
      hpx_lco_get(ld->collVal, sizeof(double), &maxVal);

      t_stop = TIME();

      if (i >= skip) {
        timer += t_stop - t_start;
      }
    }

    latency = (1.0 * timer) / iterations;

    hpx_lco_set(ld->minTime, sizeof(double), &latency, HPX_NULL, HPX_NULL);
    hpx_lco_get(ld->minTime, sizeof(double), &min_time);

    hpx_lco_set(ld->maxTime, sizeof(double), &latency, HPX_NULL, HPX_NULL);
    hpx_lco_get(ld->maxTime, sizeof(double), &max_time);

    hpx_lco_set(ld->avgTime, sizeof(double), &latency, HPX_NULL, HPX_NULL);
    hpx_lco_get(ld->avgTime, sizeof(double), &avg_time);

    if (!MYTHREAD)
      avg_time = avg_time/THREADS;

    print_data(MYTHREAD, full, size*sizeof(char), avg_time, min_time,
               max_time, iterations);
  }

  hpx_lco_set(ld->complete, 0, NULL, HPX_NULL, HPX_NULL);
  hpx_gas_unpin(target);

  return HPX_SUCCESS;
}

static int _init_action(const InitArgs *args) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->index    = args->index;
  ld->complete = args->complete;

  ld->maxSize = args->maxSize;
  ld->collVal = args->collVal;

  ld->maxTime = args->maxTime;
  ld->minTime = args->minTime;
  ld->avgTime = args->avgTime;

  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}

static int _main_action(int *args) {
  int i = 0, full=1;
  int max_msg_size = *args;
  int THREADS = HPX_LOCALITIES;

  print_header(HEADER, HPX_LOCALITY_ID, full);

  hpx_addr_t src = hpx_gas_alloc_cyclic(THREADS, max_msg_size*sizeof(char), 0);
  hpx_addr_t complete = hpx_lco_and_new(THREADS);
  hpx_addr_t done = hpx_lco_and_new(THREADS);
  hpx_addr_t collVal = hpx_lco_allreduce_new(THREADS, THREADS, sizeof(double),
                       initDouble, maxDouble);

  hpx_addr_t maxTime = hpx_lco_allreduce_new(THREADS, THREADS, sizeof(double),
                       initDouble, maxDouble);
  hpx_addr_t minTime = hpx_lco_allreduce_new(THREADS, THREADS, sizeof(double),
                       initDouble, minDouble);
  hpx_addr_t avgTime = hpx_lco_allreduce_new(THREADS, THREADS, sizeof(double),
                       initDouble, sumDouble);

  for (i = 0; i < THREADS; ++i) {
     InitArgs init = {
       .index = i,
       .maxSize = max_msg_size,
       .complete = complete,
       .collVal = collVal,
       .maxTime = maxTime,
       .minTime = minTime,
       .avgTime = avgTime
     };
     hpx_addr_t remote = hpx_addr_add(src, sizeof(Domain) * i, sizeof(Domain));
     hpx_call(remote, _init, done, &init, sizeof(init));
  }
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  for (i = 0; i < THREADS; ++i) {
    hpx_addr_t remote = hpx_addr_add(src, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(remote, _reduce, HPX_NULL, NULL, 0);
  }

  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);

  hpx_gas_free(src, HPX_NULL);
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[])
{
  int max_msg_size = DEFAULT_MAX_MESSAGE_SIZE;
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

  argc -= optind;
  argv += optind;

  if (argc > 0) {
    switch (argc) {
      case 1:
        max_msg_size = atoi(argv[0]);
        break;
      default:
        usage(stderr);
        break;
    }
  }

  int ranks = HPX_LOCALITIES;
  if (ranks < 2) {
    fprintf(stderr, "A minimum of 2 localities are required to run this test\n.");
    return 1;
  }

  HPX_REGISTER_ACTION(_main_action, &_main);
  HPX_REGISTER_ACTION(_init_action, &_init);
  HPX_REGISTER_ACTION(_reduce_action, &_reduce);

  return hpx_run(&_main, &max_msg_size, sizeof(max_msg_size));
}
