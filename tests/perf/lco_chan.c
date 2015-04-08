// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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
/// @file examples/pingpong-chan.c
/// ----------------------------------------------------------------------------

#define MAX_MSG_SIZE        (1<<22)
#define SKIP_LARGE          10
#define LOOP_LARGE          100
#define LARGE_MESSAGE_SIZE  (1<<13)

#define FIELD_WIDTH 20

static const int skip = 1000;
static const int loop = 10000;

static hpx_action_t _main   = 0;
static hpx_action_t _pinger = 0;
static hpx_action_t _ponger = 0;

char buffer[MAX_MSG_SIZE];

#define BENCHMARK "HPX COST OF CHAN LCO (ms)"
#define HEADER "# " BENCHMARK "\n"

static void _usage(FILE *stream) {
  fprintf(stream, "Usage: time_lco_chan [options]\n"
          "\t-h, this help display\n");
  hpx_print_help();
  fflush(stream);
}

static int _pinger_action(hpx_addr_t *chans) {
  int _loop = loop;
  int _skip = skip;

  fprintf(stdout, HEADER);
  fprintf(stdout, "%s%*s\n", "# Size ", FIELD_WIDTH, "Latency");

  hpx_thread_set_affinity(0);
  for (size_t size = 1; size <= MAX_MSG_SIZE; size*=2) {
    if (size > LARGE_MESSAGE_SIZE) {
      _loop = LOOP_LARGE;
      _skip = SKIP_LARGE;
    }

    hpx_time_t start;
    for (int i = 0; i < _loop + _skip; i++) {
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
    fprintf(stdout, "%-*zu%*g\n", 10, size, FIELD_WIDTH, elapsed/(1.0 * _loop));
  }
  return HPX_SUCCESS;
}

static int _ponger_action(hpx_addr_t *chans) {
  int _loop = loop;
  int _skip = skip;
  hpx_thread_set_affinity(1);
  for (size_t size = 1; size <= MAX_MSG_SIZE; size*=2) {
    if (size > LARGE_MESSAGE_SIZE) {
      _loop = LOOP_LARGE;
      _skip = SKIP_LARGE;
    }

    for (int i = 0; i < _loop + _skip; i++) {
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
  hpx_call(HPX_HERE, _pinger, done, chans, sizeof(chans));
  hpx_call(HPX_HERE, _ponger, done, chans, sizeof(chans));
  hpx_lco_wait(done);

  hpx_lco_delete(chans[0], HPX_NULL);
  hpx_lco_delete(chans[1], HPX_NULL);
  hpx_lco_delete(done, HPX_NULL);
  hpx_shutdown(0);
}

int main(int argc, char *argv[argc]) {
  return 77;
  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  int opt = 0;
  while ((opt = getopt(argc, argv, "h?")) != -1) {
    switch (opt) {
     case 'h':
      _usage(stdout);
      return 0;
     case '?':
     default:
      _usage(stderr);
      return -1;
    }
  }

  if (HPX_THREADS < 2) {
    fprintf(stderr, "This test only runs with at least 2 threads!\n");
    return -1;
  }

  HPX_REGISTER_ACTION(_main_action, &_main);
  HPX_REGISTER_ACTION(_pinger_action, &_pinger);
  HPX_REGISTER_ACTION(_ponger_action, &_ponger);
  return hpx_run(&_main, NULL, 0);
}
