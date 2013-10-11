

#include <stdio.h>
#include <inttypes.h>                           /* PRId64 */
#include <hpx.h>

hpx_context_t *ctx;
hpx_action_t   act;
hpx_timer_t    timer;
static int     nthreads;
static int     num_ranks;
static int     my_rank;

static hpx_action_t fib_action;

void fib(void *n) {
  long num = *(long*) n;


  /* handle our base case */
  if (num < 2)
    hpx_thread_exit(&num);

  /* create children parcels */
  my_rank = hpx_get_rank();
  hpx_locality_t *left = hpx_find_locality((my_rank+num_ranks-1)%num_ranks);
  hpx_locality_t *right = hpx_find_locality((my_rank+1)%num_ranks);

  long n1 = num - 1;
  hpx_thread_t *th1 = hpx_call(left, fib_action, &n1, sizeof(long));

  long n2 = num - 2;
  hpx_thread_t *th2 = hpx_call(right, fib_action, &n2, sizeof(long));

  /* wait for threads to finish */
  // ADK: need an OR gate here. Also, why not just expose the future
  //      interface and have such control constructs for them?
  long n3; hpx_thread_join(th2, (void**)&n3);
  long n4; hpx_thread_join(th1, (void**)&n4);

  long *sum = hpx_alloc(sizeof(*sum));  
  *sum = n3 + n4;
  nthreads += 2;
  hpx_thread_exit(sum);
}

int main(int argc, char *argv[]) {
  hpx_config_t cfg;
  long n;
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

#if 0
  if (localities > 0)
    hpx_config_set_localities(&cfg, localities);
#endif

  /* get the number of localities */
  num_ranks = hpx_get_num_localities();

  /* get a thread context */
//  ctx = hpx_ctx_create(&cfg);

  /* register the fib action */
  fib_action = hpx_action_register("fib", fib);

  /* get start time */
  hpx_get_time(&timer);

  /* create a fibonacci thread */
  hpx_future_t *fut = hpx_action_invoke(fib_action, (void*) n, NULL);
  
  /* wait for the thread to finish */
  hpx_thread_wait(fut);
                                       
#if 0
  printf("fib(%ld)=%ld\nseconds: %.7f\nlocalities:   %d\nthreads: %d\n",
         n, *result, hpx_elapsed_us(timer)/1e3,
	 hpx_config_get_localities(&cfg), ++nthreads);
#endif
  printf("fib(%ld)=%" PRId64 "\n", n, hpx_lco_future_get_value_i64(fut));
  printf("seconds: %.7f\n", hpx_elapsed_us(timer)/1e3);
  printf("localities:   %d\n", localities);
  printf("threads: %d\n", ++nthreads);

  /* cleanup */
//  hpx_ctx_destroy(ctx);
  return 0;
}
