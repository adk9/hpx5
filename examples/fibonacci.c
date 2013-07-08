
#include <stdio.h>
#include <hpx/timer.h>
#include <hpx/thread.h>

hpx_context_t *ctx;
hpx_timer_t    timer;
static int     nthreads;

void fib(void *n) {
  hpx_thread_t *th1;
  hpx_thread_t *th2;

  /* handle our base case */
  if ((long)n < 2) {
    return;
  }

  /* create child threads */
  th1 = hpx_thread_create(ctx, fib, (void*) (long)n - 1);
  th2 = hpx_thread_create(ctx, fib, (void*) (long)n - 2);

  /* wait for threads to finish */
  hpx_thread_join(th1, NULL);
  hpx_thread_join(th2, NULL);
  nthreads += 2;
}

int main(int argc, char * argv[]) {
  hpx_config_t cfg;
  hpx_thread_t *th;
  long n;
  uint32_t cores;

  /* validate our arguments */
  if (argc < 2) {
    fprintf(stderr, "Invalid number of cores (set to 0 to use all available cores).\n");
    return -1;
  } else if (argc < 3) {
    fprintf(stderr, "Invalid Fibonacci number.\n");
    return -2;
  } else {
    cores = atoi(argv[1]);
    n = atol(argv[2]);
  }

  /* initialize hpx runtime */
  hpx_init();

  /* set up our configuration */
  hpx_config_init(&cfg);

  if (cores > 0) {
    hpx_config_set_cores(&cfg, cores);
  }

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);

  /* get start time */
  hpx_get_time(&timer);

  /* create a fibonacci thread */
  th = hpx_thread_create(ctx, fib, (void *)n);

  /* wait for the thread to finish */
  hpx_thread_join(th, NULL);

  printf("seconds: %.7f\ncores:   %d\nthreads: %d\n", hpx_elapsed_us(timer)/1e3,
	 hpx_config_get_cores(&cfg), ++nthreads);

  /* cleanup */
  hpx_ctx_destroy(ctx);
  return 0;
}
