#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
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

static hpx_action_t fib_action = HPX_ACTION_NULL;

void
fib(long *n)
{
  /* handle our base case */
  if (*n < 2)
    hpx_thread_exit((void*)*n);

  int rank = hpx_get_my_rank();
  int ranks = hpx_get_num_ranks();

  int peers[] = {
    (rank + ranks - 1) % ranks,
    (rank + 1) % ranks
  };

  long ns[] = {
    *n - 1,
    *n - 2
  };

  hpx_addr_t futures[] = {
    hpx_future_new(sizeof(long)),
    hpx_future_new(sizeof(long))
  };

  long fns[] = {
    0,
    0
  };

  void *addrs[] = {
    &fns[0],
    &fns[1]
  };

  hpx_call(peers[0], fib_action, &ns[0], sizeof(long), futures[0]);
  hpx_call(peers[1], fib_action, &ns[1], sizeof(long), futures[1]);
  hpx_thread_wait_all(2, futures, addrs);
  hpx_future_delete(futures[0]);
  hpx_future_delete(futures[1]);

  long fn = fns[0] + fns[1];

  hpx_thread_exit((void*) fn);
}

int main(int argc, char *argv[]) {
  hpx_init(argc, argv);

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

  if (debug) {
    int i = 0;
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    printf("PID %d on %s ready for attach\n", getpid(), hostname);
    fflush(stdout);
    while (0 == i)
      sleep(5);
  }

  /* register the fib action */
  fib_action = hpx_action_register("fib", (hpx_action_handler_t)fib);
  hpx_action_registration_complete();

  /* get start time */
  hpx_time_t clock = hpx_time_now();
  long fn          = 0;
  hpx_addr_t fut   = hpx_future_new(sizeof(long));
  hpx_call(hpx_get_my_rank(), fib_action, &n, sizeof(n), &fut);
  hpx_thread_wait(fut, &fn);

  double time = hpx_time_to_us(clock - hpx_time_now())/1e3;

  printf("fib(%ld)=%ld\n", n, fn);
  printf("seconds: %.7f\n", time);
  printf("localities:   %d\n", hpx_get_num_ranks());

  /* cleanup */
  hpx_cleanup();
  return 0;
}
