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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "hpx/hpx.h"

/// ----------------------------------------------------------------------------
/// @file examples/pingpong-chan.c
/// ----------------------------------------------------------------------------

#define MAX_MSG_SIZE        (1<<22)
#define SKIP_LARGE          10
#define LOOP_LARGE          100
#define LARGE_MESSAGE_SIZE  (1<<13)

int skip = 1000;
int loop = 10000;

static hpx_action_t _main   = 0;
static hpx_action_t _pinger = 0;
static hpx_action_t _ponger = 0;

char buffer[MAX_MSG_SIZE];

static int _pinger_action(hpx_addr_t *chans) {
  hpx_thread_set_affinity(0);
  for (size_t size = 1; size <= MAX_MSG_SIZE; size*=2) {
    if (size > LARGE_MESSAGE_SIZE) {
      loop = LOOP_LARGE;
      skip = SKIP_LARGE;
    }

    hpx_time_t start;
    for (int i = 0; i < loop + skip; i++) {
      if(i == skip)
        start = hpx_time_now();

      hpx_addr_t done = hpx_lco_future_new(0);
      hpx_lco_chan_send(chans[0], size, buffer, done, HPX_NULL);
      hpx_lco_wait(done);
      hpx_lco_delete(done, HPX_NULL);
      void *rbuf;
      hpx_lco_chan_recv(chans[1], NULL, &rbuf);
      free(rbuf);
    }
    double elapsed = hpx_time_elapsed_ms(start);
    printf("Size %lu, Latency: %g\n", size, elapsed/(1.0 * loop));
  }
  return HPX_SUCCESS;
}

static int _ponger_action(hpx_addr_t *chans) {
  hpx_thread_set_affinity(1);
  for (size_t size = 1; size <= MAX_MSG_SIZE; size*=2) {
    if (size > LARGE_MESSAGE_SIZE) {
      loop = LOOP_LARGE;
      skip = SKIP_LARGE;
    }

    for (int i = 0; i < loop + skip; i++) {
      void *rbuf;
      hpx_lco_chan_recv(chans[0], NULL, &rbuf);
      free(rbuf);
      hpx_addr_t done = hpx_lco_future_new(0);
      hpx_lco_chan_send(chans[1], size, buffer, done, HPX_NULL);
      hpx_lco_wait(done);
      hpx_lco_delete(done, HPX_NULL);
    }
  }
  return HPX_SUCCESS;
}


static int _main_action(void *args) {
  hpx_addr_t done = hpx_lco_and_new(2);

  hpx_addr_t chans[2] = {
    hpx_lco_chan_new(), // pinger write ponger read
    hpx_lco_chan_new()  // ponger write pinger read
  };

  // spawn the pinger and ponger threads.
  hpx_call(HPX_HERE, _pinger, chans, sizeof(chans), done);
  hpx_call(HPX_HERE, _ponger, chans, sizeof(chans), done);
  hpx_lco_wait(done);

  hpx_lco_delete(chans[0], HPX_NULL);
  hpx_lco_delete(chans[1], HPX_NULL);
  hpx_lco_delete(done, HPX_NULL);
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

  if (HPX_THREADS < 2) {
    fprintf(stderr, "This test only runs with at least 2 threads!\n");
    return -1;
  }

  _main   = HPX_REGISTER_ACTION(_main_action);
  _pinger = HPX_REGISTER_ACTION(_pinger_action);
  _ponger = HPX_REGISTER_ACTION(_ponger_action);
  return hpx_run(_main, NULL, 0);
}
