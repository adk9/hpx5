
#include <stdio.h>
#include <hpx/timer.h>
#include <hpx/thread.h>

hpx_context_t *ctx;
hpx_action_t   act;
hpx_timer_t    timer;
static int     nthreads;
static int     num_ranks;
static int     my_rank;

void fib(void *n) {
  long *n1, *n2, num, sum;
  hpx_thread_t *th1;
  hpx_thread_t *th2;
  hpx_locality_t *left, *right;

  num = (long) n;
  /* handle our base case */
  if (num < 2)
    hpx_thread_exit(&num);

  /* create children parcels */
  my_rank = hpx_get_rank();
  left = hpx_get_locality((my_rank+num_ranks-1)%num_ranks);
  right = hpx_get_locality((my_rank+1)%num_ranks);

  th1 = hpx_call(left, "fib", (void*) num-1, sizeof(long));
  th2 = hpx_call(right, "fib", (void*) num-2, sizeof(long));

  /* wait for threads to finish */
  // ADK: need an OR gate here. Also, why not just expose the future
  //      interface and have such control constructs for them?
  hpx_thread_join(th2, (void**) &n2);
  hpx_thread_join(th1, (void**) &n1);
  sum = *n1 + *n2;
  nthreads += 2;
  hpx_thread_exit(&sum);
}

int main(int argc, char * argv[]) {
  hpx_config_t cfg;
  long n, *result;
  uint32_t localities;

  /* validate our arguments */
  if (argc < 2) {
    fprintf(stderr, "Invalid number of localities (set to 0 to use all available localities).\n");
    return -1;
  } else if (argc < 3) {
    fprintf(stderr, "Invalid Fibonacci number.\n");
    return -2;
  } else {
    localities = atoi(argv[1]);
    n = atol(argv[2]);
  }

  /* initialize hpx runtime */
  hpx_init();

  /* set up our configuration */
  hpx_config_init(&cfg);

  if (cores > 0)
    hpx_config_set_localities(&cfg, localities);

  /* get the number of localities */
  num_ranks = hpx_get_num_localities();

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);

  /* register the fib action */
  hpx_action_register("fib", fib, &act);

  /* get start time */
  hpx_get_time(&timer);

  /* create a fibonacci thread */
  hpx_action_invoke(&act, (void*) n, (void**) &result);

  printf("fib(%ld)=%ld\nseconds: %.7f\ncores:   %d\nthreads: %d\n",
         n, *result, hpx_elapsed_us(timer)/1e3,
	 hpx_config_get_cores(&cfg), ++nthreads);

  /* cleanup */
  hpx_ctx_destroy(ctx);
  return 0;
}
