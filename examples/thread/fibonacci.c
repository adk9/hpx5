
#include <stdio.h>
#include <hpx/config.h>
#include <hpx/timer.h>
#include <hpx/ctx.h>
#include <hpx/thread.h>

hpx_context_t *ctx;
hpx_timer_t    timer;
static int     nthreads;

void fib(void *n) {
  long *n1, *n2, num, sum;
  hpx_thread_t *th1;
  hpx_thread_t *th2;

  num = (long) n;
  /* handle our base case */
  if (num < 2)
    hpx_thread_exit(&num);

  /* create child threads */
  th1 = hpx_thread_create(ctx, 0, fib, (void*) num-1);
  th2 = hpx_thread_create(ctx, 0, fib, (void*) num-2);

  /* wait for threads to finish */
  // ADK: need an OR gate here. Also, why not just expose the future
  //      interface and have such control constructs for them?
  hpx_thread_join(th2, (void**) &n2);
  hpx_thread_join(th1, (void**) &n1);
  sum = *n1 + *n2;
  nthreads += 2;
  hpx_thread_exit(&sum);
}

int main(int argc, char *argv[]) {
  hpx_config_t cfg;
  hpx_thread_t *th;
  long n, *result;
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
  hpx_config_set_thread_suspend_policy(&cfg, HPX_CONFIG_THREAD_SUSPEND_SRV_LOCAL);

  if (cores > 0) {
    hpx_config_set_cores(&cfg, cores);
  }

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);

  /* get start time */
  hpx_get_time(&timer);

  /* create a fibonacci thread */
  th = hpx_thread_create(ctx, 0, fib, (void *)n);

  /* wait for the thread to finish */
  hpx_thread_join(th, (void**)&result);

  printf("fib(%ld)=%ld\nseconds: %.7f\ncores:   %d\nthreads: %d\n",
         n, *result, hpx_elapsed_us(timer)/1e3,
	 hpx_config_get_cores(&cfg), ++nthreads);

  /* cleanup */
  hpx_ctx_destroy(ctx);

  hpx_cleanup();
  return 0;
}
