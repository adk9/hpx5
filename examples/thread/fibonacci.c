/*
  ====================================================================
  High Performance ParalleX Library (libhpx)

  The thread-based fibonacci example
  examples/thread/fibonacci.c

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/

#include <stdio.h>                              /* sprintf */
#include <inttypes.h>                           /* PRI... */
#include "hpx/hpx.h"
#include <sync/sync.h>                          /* sync access */

/**
 * This file defines a basic fibonacci example, that uses HPX threads to
 * perform the naive recursive tree form of fibonnaci.
 */
static const char usage[] = "fibonacci cores n\n"
                            "\t[cores=0=>use all available cores]\n";
static int nthreads;                            /**< for output */


/**
 * This fib action decomposes the problem of computing fib(n) into computing
 * fib(n - 1) + fib(n - 2). It uses the action void* argument to directly pass
 * n (and n -1, n -2, etc), rather than using it to pass an address of n, and
 * retrieved the computed results using futures.
 *
 * @param args The current number to compute, cast as void*.
 * @return The computed fib(n) directly, NOT an address.
 */
static
void fib(void *args)
{
  long n = (long) args;

  /* handle our base case */
  if (n < 2)
    hpx_thread_exit(args);

  /* create child threads */
  hpx_future_t *f1 = NULL;
  hpx_future_t *f2 = NULL;
  hpx_thread_create(NULL, 0, fib, (void*) (n - 1), &f1, NULL);
  hpx_thread_create(NULL, 0, fib, (void*) (n - 2), &f2, NULL);

  /* wait for threads to finish */
  hpx_thread_wait(f1);
  hpx_thread_wait(f2);

  /* get the values */
  long n1 = (long) hpx_lco_future_get_value(f1);
  long n2 = (long) hpx_lco_future_get_value(f2);

  /* update the number of threads */
  sync_fadd(&nthreads, 2, SYNC_SEQ_CST);

  /* return the sum directly */
  hpx_thread_exit((void *) (n1 + n2));
}

/**
 * The main function parses the command line, sets up the HPX runtime system,
 * and initiates the first HPX thread to perform fib(n).
 *
 * @param argc number of strings
 * @param argv[0] fibonacci
 * @param argv[1] number of cores to use, '0' means use all
 * @param argv[2] n
 */
int
main(int argc, char *argv[])
{
  /* validate our arguments */
  if (argc < 3) {
    fprintf(stderr, usage);
    return -2;
  }

  uint32_t cores = atoi(argv[1]);
  long n = atol(argv[2]);

  /* configure HPX */
  hpx_config_t cfg = {0};
  hpx_config_init(&cfg);
  hpx_config_set_thread_suspend_policy(&cfg,
      HPX_CONFIG_THREAD_SUSPEND_SRV_LOCAL);

  /* set the requested number of cores, if cores == 0, HPX uses max */
  if (cores)
    hpx_config_set_cores(&cfg, cores);

  /* initialize hpx runtime */
  hpx_init(&cfg);

  /* get start time */
  hpx_timer_t timer;
  hpx_get_time(&timer);

  /* initialize thread count */
  nthreads = 1;

  /* create a fibonacci thread */
  hpx_future_t *f;
  hpx_thread_create(NULL, 0, fib, (void*) n, &f, NULL);

  /* wait for the thread to finish */
  hpx_thread_wait(f);
  long fib_n = (long) hpx_lco_future_get_value(f);

  /* get the time */
  float ms = hpx_elapsed_us(timer) / 1e3;

  printf("fib(%ld)=%ld\n", n, fib_n);
  printf("seconds: %.7f\n", ms);
  printf("cores:   %d\n", hpx_config_get_cores(__hpx_global_cfg));
  printf("threads: %d\n", nthreads);

  /* cleanup */
  hpx_cleanup();

  return 0;
}
