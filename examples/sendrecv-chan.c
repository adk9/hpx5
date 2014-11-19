// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "hpx/hpx.h"

/// ----------------------------------------------------------------------------
/// @file examples/sendrecv-chan.c
/// ----------------------------------------------------------------------------

#define LIMIT 24

static int counts[LIMIT] = {
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

static int _receiver_action(hpx_addr_t *args) {
  hpx_addr_t chan = *args;
  int avg = 10000;

  hpx_thread_set_affinity(1);

  for (int i=0;i<LIMIT;++i) {
    for (int k=0;k<avg;k++) {
      hpx_addr_t done = hpx_lco_future_new(0);
      // 10.6 microseconds
      // int delay = 1000;
      // hpx_call(HPX_HERE, _worker, &delay, sizeof(delay), done);
      // 106 microseconds
      int delay = 10000;
      hpx_call(HPX_HERE, _worker, &delay, sizeof(delay), done);
      double *buf;
      hpx_lco_chan_recv(chan, NULL, (void**)&buf);
      assert(buf);
      hpx_lco_wait(done);
      hpx_lco_delete(done, HPX_NULL);
      free(buf);
    }
  }
  hpx_thread_continue(0, NULL);
}


static int _main_action(void *args) {
  int avg = 10000;

  hpx_thread_set_affinity(0);

  hpx_time_t tick = hpx_time_now();
  printf(" Tick: %g\n", hpx_time_us(tick));

  hpx_addr_t chan = hpx_lco_chan_new();
  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_call(HPX_HERE, _receiver, &chan, sizeof(chan), done);

  for (int i=0;i<LIMIT;++i) {
    double *buf = (double *) malloc(sizeof(double)*counts[i]);
    for (int j=0;j<counts[i];++j)
      buf[j] = j*rand();

    hpx_time_t t1 = hpx_time_now();
    hpx_addr_t sfut = hpx_lco_and_new(avg);
    // sfut just controls local completion, these sends may occur out-of-order
    for (int k=0; k<avg; ++k)
      hpx_lco_chan_send(chan, sizeof(double)*counts[i], buf, sfut, HPX_NULL);
    hpx_lco_wait(sfut);
    hpx_lco_delete(sfut, HPX_NULL);
    double elapsed = hpx_time_elapsed_ms(t1);
    printf("%d, %d: Elapsed: %g\n", i, counts[i], elapsed/avg);
    free(buf);
  }

  hpx_lco_wait(done);
  hpx_lco_delete(chan, HPX_NULL);
  hpx_lco_delete(done, HPX_NULL);
  hpx_shutdown(0);
}


static void usage(FILE *f) {
  fprintf(f, "Usage: sendrecv-chan [options]\n"
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

  if (HPX_LOCALITIES != 1 || HPX_THREADS < 2) {
    fprintf(stderr, "This test only runs on 1 locality with at least 2 threads!\n");
    return -1;
  }

  HPX_REGISTER_ACTION(&_main, _main_action);
  HPX_REGISTER_ACTION(&_worker, _worker_action);
  HPX_REGISTER_ACTION(&_receiver, _receiver_action);
  return hpx_run(&_main, NULL, 0);
}
