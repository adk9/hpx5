#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "hpx/hpx.h"

static int counts[24] = {
  1,
  2,
  3,
  4,
  25,
  50,
  75,
  100,
  125,
  500,
  1000,
  2000,
  3000,
  4000,
  25000,
  50000,
  75000,
  100000,
  125000,
  500000,
  1000000,
  2000000,
  3000000,
  4000000
};

static hpx_action_t _main     = 0;
static hpx_action_t _worker   = 0;
static hpx_action_t _receiver = 0;

static int _worker_action(int *args) {
  int delay = *args;
  double volatile d = 0.;

  hpx_thread_set_affinity(1);

  for (int i=0;i<delay;++i) {
    d += 1./(2.*i+1.);
  }
  hpx_thread_continue(0, NULL);
}

static int _receiver_action(double *args) {
  double *buf = args;
  int avg = 10000;

  hpx_thread_set_affinity(1);

  for (int k=0;k<avg;k++) {
    hpx_addr_t done = hpx_lco_future_new(0);
    // 10.6 microseconds
    // int delay = 1000;
    // hpx_call(HPX_HERE, _worker, &delay, sizeof(delay), done);
    // 106 microseconds
    int delay = 10000;
    hpx_call(HPX_HERE, _worker, &delay, sizeof(delay), done);
    hpx_lco_wait(done);
    hpx_lco_delete(done, HPX_NULL);
  }
  return HPX_SUCCESS;
}


static int _main_action(void *args) {
  int avg = 10000;

  hpx_thread_set_affinity(0);

  hpx_time_t tick = hpx_time_now();
  printf(" Tick: %g\n", hpx_time_us(tick));

  for (int i=0;i<24;++i) {
    double *buf = (double *) malloc(sizeof(double)*counts[i]);
    for (int j=0;j<counts[i];++j)
      buf[j] = j*rand();

    hpx_time_t t1 = hpx_time_now();
    for (int k=0; k<avg; ++k)
      hpx_call(HPX_HERE, _receiver, buf, sizeof(double)*counts[i], HPX_NULL);

    double elapsed = hpx_time_elapsed_ms(t1);
    printf(" Elapsed: %g\n",elapsed/avg);
    free(buf);
  }

  hpx_shutdown(0);
}


static void usage(FILE *f) {
  fprintf(f, "Usage: [options]\n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, show help\n");
}


int main(int argc, char *argv[argc]) {
  hpx_config_t cfg = {
    .cores         = 0,
    .threads       = 0,
    .stack_bytes   = 0,
    .gas           = HPX_GAS_NOGLOBAL
  };

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:Dh")) != -1) {
    switch (opt) {
      case 'c':
        cfg.cores = atoi(optarg);
        break;
      case 't':
        cfg.threads = atoi(optarg);
        break;
      case 'D':
        cfg.wait = HPX_WAIT;
        cfg.wait_at = HPX_LOCALITY_ALL;
        break;
      case 'd':
        cfg.wait = HPX_WAIT;
        cfg.wait_at = atoi(optarg);
        break;
      case 'h':
        usage(stdout);
        return 0;
      case '?':
      default:
        usage(stderr);
        return -1;
    }
  }

  if (hpx_init(&cfg)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  if (HPX_LOCALITIES != 1 || HPX_THREADS < 2) {
    fprintf(stderr, "This test only runs on 1 locality with at least 2 threads!\n");
    return -1;
  }

  _main     = HPX_REGISTER_ACTION(_main_action);
  _worker   = HPX_REGISTER_ACTION(_worker_action);
  _receiver = HPX_REGISTER_ACTION(_receiver_action);
  return hpx_run(_main, NULL, 0);
}
