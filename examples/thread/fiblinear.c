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

#include <unistd.h>
#include <stdio.h>
#include <hpx.h>

/**
 * This file defines a smarter fibonacci example, that uses many malloced
 * futures to avoid recomputing larger parts of the recursive tree.
 */

static hpx_future_t **numbers = NULL;           /**< the future array  */

/**
 * Prints a usage string to stderr.
 *
 * @returns -1, for error cases where the caller wants to say "return usage()."
 */
static int usage() {
   fprintf(stderr,
           "\n"
           "Usage: fibsmarter [-ch] n\n"
           "\t n\tfibonacci number to compute\n"
           "\t-c\tnumber of cores\n"
           "\t-h\tthis help display\n"
           "\n");
   return -1;
}

/**
 * The main fib action, computes a fib number for n by waiting on the global
 * future for n - 1 and n - 2.
 *
 * This action only actually waits on globals[n - 1] because we know that
 * globals[n - 1] already depends on globals[n - 2]. Also, it frees the future
 * for globals[n - 2] because we know no further actions will need to look at
 * it, and this gives us some parallelism for freeing.
 *
 * @param[in] args The value for n, directly, not a pointer.
 */
void fib(void *args) {
  unsigned long n = (unsigned long)args;
  unsigned long fibn = n;
  if (n > 1) {
    hpx_thread_wait(numbers[n-1]);
    fibn = (unsigned long) hpx_lco_future_get_value(numbers[n-2]) +
      (unsigned long) hpx_lco_future_get_value(numbers[n-1]);
    hpx_lco_future_destroy(numbers[n-2]);
  }
  hpx_lco_future_set_value(numbers[n], (void*) fibn);
}


/**
 * The main function processes the command line, initializes HPX, and spawns
 * all of the threads, storing their returned futures in globals. Then it waits
 * for the final falue of fib(n) and prints out the result.
 */
int main(int argc, char *const argv[]) {
  unsigned long cores = 0;                       /* # of cores from -c */
  unsigned long n = 0;                           /* fib(n) from argv[] */

  /* handle command line */
  int opt = 0;
  while ((opt = getopt(argc, argv, "c:h")) != -1) {
    switch (opt) {
    case 'c': cores = strtol(optarg, NULL, 10); break;
    case 'h': usage(); break;
    case '?':
    default: return usage();
    }
  }

  argc -= optind;
  argv += optind;

  if (argc == 0) {
    fprintf(stderr, "Missing fib number to compute\n");
    return usage();
  }
  n = strtol(argv[0], NULL, 10);

  /* configure HPX */
  hpx_config_t cfg = {0};
  hpx_config_init(&cfg);
  hpx_config_set_thread_suspend_policy(&cfg,
                                       HPX_CONFIG_THREAD_SUSPEND_SRV_LOCAL);

  /* set the requested number of cores, if cores == 0, HPX uses max */
  if (cores)
    hpx_config_set_cores(&cfg, cores);
  cores = hpx_config_get_cores(&cfg);

  /* intialize HPX */
  hpx_init(&cfg);

  /* report our run type */
  printf("Computing fib(%lu) on %lu cores\n", n, cores);

  /* get start time */
  hpx_timer_t timer;
  hpx_get_time(&timer);

  /* allocate the array we'll use for storage */
  if ((numbers = malloc((n + 1) * sizeof(numbers[0]))) == NULL) {
    fprintf(stderr, "Could not alloc number array.\n");
    return -1;
  }

  /* spawn a single thread for each number */
  for (unsigned long i = 0, e = n + 1; i < e; ++i)
    hpx_thread_create(NULL, HPX_THREAD_OPT_DETACHED, fib, (void*) i,
                      &numbers[i], NULL);

  /* wait for the last thread to complete */
  hpx_thread_wait(numbers[n]);
  unsigned long fibn = (unsigned long) hpx_lco_future_get_value(numbers[n]);

  /* clean up the last two futures, and the array */
  hpx_lco_future_destroy(numbers[n]);
  if (n > 0)
    hpx_lco_future_destroy(numbers[n - 1]);
  free(numbers);

  /* get the elapsed time before we start calling printf */
  float ms = hpx_elapsed_us(timer) / 1e3;

  printf("fib(%lu)=%lu\n", n, fibn);
  printf("seconds: %.7f\n", ms);
  printf("cores:   %lu\n", cores);
  /* printf("threads: %lu\n", n + 2); */

  hpx_cleanup();
  return 0;
}
