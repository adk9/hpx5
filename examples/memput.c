#define BENCHMARK "OSU HPX MEMPUT Test"

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "hpx/hpx.h"

#define MAX_MSG_SIZE        (1<<22)
#define SKIP_LARGE          10
#define LOOP_LARGE          100
#define LARGE_MESSAGE_SIZE  (1<<13)

int skip = 1000;
int loop = 10000;

#ifdef PACKAGE_VERSION
#   define HEADER "# " BENCHMARK " v" PACKAGE_VERSION "\n"
#else
#   define HEADER "# " BENCHMARK "\n"
#endif

#ifndef FIELD_WIDTH
#   define FIELD_WIDTH 20
#endif

#ifndef FLOAT_PRECISION
#   define FLOAT_PRECISION 2
#endif

void wtime(double *t)
{
  static int sec = -1;
  struct timeval tv;
  gettimeofday(&tv, (void *)0);
  if (sec < 0) sec = tv.tv_sec;
  *t = (tv.tv_sec - sec)*1.0e+6 + tv.tv_usec;
}

static hpx_action_t _main = 0;
static hpx_action_t _init_array = 0;

static int _init_array_action(size_t *args) {
  size_t n = *args;
  hpx_addr_t target = hpx_thread_current_target();
  char *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  for(int i = 0; i < n; i++)
    local[i] = (HPX_LOCALITY_ID == 0) ? 'a' : 'b';
  HPX_THREAD_CONTINUE(local);
}


static int _main_action(void *args) {
  double t_start = 0.0, t_end = 0.0;
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peerid = (rank+1)%size;
  int i;

  if (size == 1) {
    fprintf(stderr, "This test requires at least two HPX threads\n");
    hpx_shutdown(HPX_ERROR);
  }

  hpx_addr_t data = hpx_gas_global_alloc(size, MAX_MSG_SIZE*2);
  hpx_addr_t remote = hpx_addr_add(data, MAX_MSG_SIZE*2 * peerid, MAX_MSG_SIZE*2);

  fprintf(stdout, HEADER);
  fprintf(stdout, "# [ pairs: %d ]\n", size/2);
  fprintf(stdout, "%-*s%*s\n", 10, "# Size", FIELD_WIDTH, "Latency (us)");
  fflush(stdout);

  for (size_t size = 1; size <= MAX_MSG_SIZE; size*=2) {
    char *local;

    hpx_addr_t rfut = hpx_lco_future_new(sizeof(void*));
    hpx_call(remote, _init_array, &size, sizeof(size), rfut);
    hpx_call_sync(data, _init_array, &size, sizeof(size), &local, sizeof(local));
    hpx_lco_wait(rfut);
    hpx_lco_delete(rfut, HPX_NULL);

    if (size > LARGE_MESSAGE_SIZE) {
      loop = LOOP_LARGE;
      skip = SKIP_LARGE;
    }

    for (i = 0; i < loop + skip; i++) {
      if(i == skip)
        wtime(&t_start);

      hpx_addr_t done = hpx_lco_future_new(0);
      hpx_gas_memput(remote, local, size, HPX_NULL, done);
      hpx_lco_wait(done);
      hpx_lco_delete(done, HPX_NULL);
    }

    wtime(&t_end);

    double latency = (t_end - t_start)/(1.0 * loop);
    fprintf(stdout, "%-*lu%*.*f\n", 10, size, FIELD_WIDTH,
            FLOAT_PRECISION, latency);
    fflush(stdout);
  }
  hpx_shutdown(HPX_SUCCESS);
}

static void usage(FILE *f) {
  fprintf(f, "Usage: memput [options]\n"
          "\t-h, show help\n");
  hpx_print_help();
  fflush(f);
}

int main(int argc, char *argv[argc]) {

  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  int opt = 0;
  while ((opt = getopt(argc, argv, "h?")) != -1) {
    switch (opt) {
     case 'h':
      usage(stdout);
      return 0;
     case '?':
     default:
      usage(stderr);
      return -1;
    }
  }

  if (HPX_LOCALITIES < 2) {
    fprintf(stderr, "A minimum of 2 localities are required to run this test.\n");
    return -1;
  }

  _main       = HPX_REGISTER_ACTION(_main_action);
  _init_array = HPX_REGISTER_ACTION(_init_array_action);
  return hpx_run(_main, NULL, 0);
}
