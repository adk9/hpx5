
#include <stdio.h>
#include <math.h>
#include <hpx/thread.h>

#ifdef __APPLE__
  #include <mach/mach_time.h>
#endif

#ifdef __linux__
  #include <time.h>
#endif

hpx_context_t * ctx;

void fib(unsigned int n) {
  hpx_thread_t * th1;
  hpx_thread_t * th2;

  /* handle our base case */
  if (n < 2) {
    return;
  }

  /* create child threads */
  th1 = hpx_thread_create(ctx, fib, (void *) (n - 1));
  th2 = hpx_thread_create(ctx, fib, (void *) (n - 2));

  /* wait for threads to finish */
  hpx_thread_join(th1, NULL);
  hpx_thread_join(th2, NULL);
}


int main(int argc, char * argv[]) {
  hpx_config_t cfg;
  hpx_thread_t * th;
  unsigned int n;
  uint32_t cores;

#ifdef __APPLE__
  static mach_timebase_info_data_t tbi;
  uint64_t begin_ts;
  uint64_t end_ts;

  if (tbi.denom == 0) {
    (void) mach_timebase_info(&tbi);
  }
#endif

#ifdef __linux__
  struct timespec begin_ts;
  struct timespec end_ts;
#endif

  /* validate our arguments */
  if (argc < 2) {
    fprintf(stderr, "Invalid number of cores (set to 0 to use all available cores).\n");
    return -1;
  } else if (argc < 3) {
    fprintf(stderr, "Invalid Fibonacci number.\n");
    return -2;
  } else {
    cores = atoi(argv[1]);
    n = atoi(argv[2]);
  }

  /* set up our configuration */
  hpx_config_init(&cfg);

  if (cores > 0) {
    hpx_config_set_cores(&cfg, cores);
  }

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);

#ifdef __APPLE__
  begin_ts = mach_absolute_time();
#endif

#ifdef __linux__
  clock_gettime(CLOCK_MONOTONIC, &begin_ts);
#endif

  /* create a fibonacci thread */
  th = hpx_thread_create(ctx, fib, (void *) n);

  /* wait for the thread to finish */
  hpx_thread_join(th, NULL);

#ifdef __APPLE__
  end_ts = mach_absolute_time();

  printf("seconds: %.7f\ncores:   %d\nthreads: %ld\n", (((end_ts - begin_ts) * tbi.numer / tbi.denom) / 1000000000.0),
	 hpx_config_get_cores(&cfg), (unsigned long) pow(2, n) + 1);
#endif

#ifdef __linux__
  clock_gettime(CLOCK_MONOTONIC, &end_ts);
  unsigned long elapsed = ((end_ts.tv_sec * 1000000000) + end_ts.tv_nsec) - ((begin_ts.tv_sec * 1000000000) + begin_ts.tv_nsec);

  printf("seconds: %.7f\ncores:   %d\nthreads: %ld\n", (elapsed / 1000000000.0),
	 hpx_config_get_cores(&cfg), (unsigned long) pow(2, n) + 1);
#endif

  /* cleanup */
  hpx_ctx_destroy(ctx);

  return 0;
}
