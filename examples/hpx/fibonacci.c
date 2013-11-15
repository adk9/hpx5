#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>                           /* PRId64 */
#include <hpx.h>

/**
 * Prints a usage string to stderr.
 *
 * @returns -1, for error cases where the caller wants to say "return usage()."
 */
static int usage() {
   fprintf(stderr,
           "\n"
           "Usage: fibonaccihpx [-cldh] n\n"
           "\t n\tfibonacci number to compute\n"
           "\t-c\tnumber of cores\n"
           "\t-d\twait for debugger\n"
           "\t-h\tthis help display\n"
           "\n");
   return -1;
}

static int num_ranks = -1;
static hpx_action_t fib_action = HPX_ACTION_NULL;

void
fib(long *n)
{
  /* handle our base case */
  if (*n < 2)
    hpx_thread_exit((void*)*n);

  int my_rank = hpx_get_rank();
  hpx_locality_t *l = hpx_locality_from_rank((my_rank + num_ranks - 1) % num_ranks);
  hpx_locality_t *r = hpx_locality_from_rank((my_rank + 1) % num_ranks);
  
  long n1 = *n - 1;
  long n2 = *n - 2;
  
  hpx_future_t *ffn1 = NULL;
  hpx_future_t *ffn2 = NULL;
  
  hpx_call(l, fib_action, &n1, sizeof(n1), &ffn1);
  hpx_call(r, fib_action, &n2, sizeof(n2), &ffn2);
  
  hpx_thread_wait(ffn1);
  hpx_thread_wait(ffn2);

  long fn1 = (long) hpx_lco_future_get_value(ffn1);
  long fn2 = (long) hpx_lco_future_get_value(ffn2);
  long fn = fn1 + fn2;
  
  hpx_thread_exit((void*) fn);
}

int
main(int argc, char *argv[])
{
  unsigned long cores = 0;                      /* # of cores from -c */
  bool debug = false;                           /* flag from -d */
  unsigned long n = 0;                          /* fib(n) from argv */

  /* handle command line */
  int opt = 0;
  while ((opt = getopt(argc, argv, "c:dh")) != -1) {
    switch (opt) {
    case 'c':
      cores = strtol(optarg, NULL, 10);
      break;
    case 'd':
      debug = true;
      break;
    case 'h':
      usage();
      break;
    case '?':
    default:
      return usage();
    }
  }

  argc -= optind;
  argv += optind;

  if (argc == 0) {
    fprintf(stderr, "Missing fib number to compute\n");
    return usage();
  }
  n = strtol(argv[0], NULL, 10);

  /* initialize hpx runtime */
  hpx_init();

  if (debug) {
    int i = 0;
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    printf("PID %d on %s ready for attach\n", getpid(), hostname);
    fflush(stdout);
    while (0 == i)
      sleep(5);
  }

  num_ranks = hpx_get_num_localities();

  /* register the fib action */
  fib_action = hpx_action_register("fib", (hpx_func_t)fib);
  hpx_action_registration_complete();
  
  /* get start time */
  hpx_timer_t timer;
  hpx_get_time(&timer);

  hpx_future_t *fut;
  hpx_action_invoke(fib_action, &n, &fut);
  hpx_thread_wait(fut);

  long result = (long) hpx_lco_future_get_value_i64(fut);
  printf("fib(%ld)=%ld\n", n, result);

  double time = hpx_elapsed_us(timer)/1e3;
  printf("seconds: %.7f\n", time);
         
  printf("localities:   %d\n", num_ranks);

  /* cleanup */
  hpx_cleanup();
  return 0;
}
