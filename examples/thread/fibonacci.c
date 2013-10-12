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
#include <hpx.h>

/**
 * This file defines a basic fibonacci example, that uses HPX threads to
 * perform the naive recursive tree form of fibonnaci.
 */
static const char usage[] = "fibonacci cores n\n"
                            "\t[cores=0=>use all available cores]\n";
static hpx_context_t *ctx;                      /**< a shared thread context */
static int            nthreads;                 /**< for output */

/**
 * This fib action decomposes the problem of computing fib(n) into computing
 * fib(n - 1) + fib(n - 2). It uses the action void* argument to directly pass
 * n (and n -1, n -2, etc), rather than using it to pass an address of n, and
 * retrieved the computed results directly through thread joins.
 *
 * @param args The current number to compute, cast as void*.
 * @return The copmuted fib(n) directly, NOT an address.
 */
static void fib(void *args) {
  long n = (long) args;
  
  /* handle our base case */
  if (n < 2)
    hpx_thread_exit(args);

  /* create child threads */
  hpx_thread_t *t1; hpx_thread_create(ctx, 0, fib, (void*) (n - 1), &t1);
  hpx_thread_t *t2; hpx_thread_create(ctx, 0, fib, (void*) (n - 2), &t2);

  /* wait for threads to finish */
  long n1; hpx_thread_join(t1, (void**) &n1);
  long n2; hpx_thread_join(t2, (void**) &n2);

  /* update the number of threads */
  __atomic_fetch_add(&nthreads, 2, __ATOMIC_SEQ_CST);
  
  /* return the sum directly */
  hpx_thread_exit((void *) (n1 + n2));
}

/**
 * This fib action decomposes the problem of computing fib(n) into computing
 * fib(n - 1) + fib(n - 2). It uses the action void* argument to directly pass
 * n (and n -1, n -2, etc), rather than using it to pass an address of n, and
 * retrieved the computed results using futures.
 *
 * @param args The current number to compute, cast as void*.
 * @return The copmuted fib(n) directly, NOT an address.
 */
static void fib_futures(void *args) {
  long n = (long) args;
  
  /* handle our base case */
  if (n < 2)
    hpx_thread_exit(args);

  /* create child threads */
  hpx_future_t *f1 = hpx_thread_create(ctx, 0, fib_futures, (void*) (n - 1), NULL);
  hpx_future_t *f2 = hpx_thread_create(ctx, 0, fib_futures, (void*) (n - 2), NULL);

  /* wait for threads to finish */
  hpx_thread_wait(f1);
  hpx_thread_wait(f2);

  /* get the values */
  long n1 = (long) hpx_lco_future_get_value(f1);
  long n2 = (long) hpx_lco_future_get_value(f2);

  /* update the number of threads */
  __atomic_fetch_add(&nthreads, 2, __ATOMIC_SEQ_CST);
  
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
int main(int argc, char *argv[]) {
  
  /* validate our arguments */
  if (argc < 3) {
    fprintf(stderr, usage);
    return -2;
  }
  
  uint32_t cores = atoi(argv[1]);
  long n = atol(argv[2]);

  /* initialize hpx runtime */
  hpx_init();

  /* set up our configuration */
  hpx_config_t cfg;
  hpx_config_init(&cfg);
  hpx_config_set_thread_suspend_policy(&cfg, HPX_CONFIG_THREAD_SUSPEND_SRV_LOCAL);

  if (cores > 0)
    hpx_config_set_cores(&cfg, cores);

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);

  /* get start time */
  hpx_timer_t timer; hpx_get_time(&timer);

  /* initialize thread count */
  nthreads = 1;
  
  /* create a fibonacci thread */
  hpx_func_t f = (argc > 5) ? fib : fib_futures;
  hpx_thread_t *t; hpx_thread_create(ctx, 0, f, (void*) n, &t);

  /* wait for the thread to finish */
  long fib_n; hpx_thread_join(t, (void**) &fib_n);
  
  printf("fib(%ld)=%ld\n", n, fib_n);
  printf("seconds: %.7f\n", hpx_elapsed_us(timer) / 1e3);
  printf("cores:   %d\n", hpx_config_get_cores(&cfg));
  printf("threads: %d\n", nthreads);

  /* cleanup */
  hpx_ctx_destroy(ctx);
  hpx_cleanup();
  
  return 0;
}
