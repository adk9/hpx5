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
/// @file examples/allredice-chan.c
///
/// This is an allreduce collective implementation using the HPX channel LCO.
/// ----------------------------------------------------------------------------

static hpx_action_t _main      = 0;
static hpx_action_t _allreduce = 0;


static int _allreduce_action(hpx_addr_t *args) {
  hpx_addr_t channels = *args;

  double buf = rand();
  hpx_addr_t root = hpx_lco_chan_array_at(channels, 0);
  hpx_lco_chan_send(root, &buf, sizeof(buf), HPX_NULL);

  hpx_addr_t chan = hpx_lco_chan_array_at(channels, HPX_LOCALITY_ID);
  double *result = hpx_lco_chan_recv(chan, sizeof(*result));
  assert(result);
  printf("rank %d (in %f, out %f).\n", HPX_LOCALITY_ID, buf, *result);
  free(result);
  hpx_thread_continue(0, NULL);
}


static int _main_action(void *args) {
  hpx_time_t tick = hpx_time_now();
  printf(" Tick: %g\n", hpx_time_us(tick));

  hpx_addr_t rank;
  int ranks = HPX_LOCALITIES;

  // initialize persistent threads---one per locality.
  hpx_addr_t channels = hpx_lco_chan_array_new(HPX_LOCALITIES, 1);
  hpx_addr_t and = hpx_lco_and_new(ranks-1);
  for (int k=0; k<ranks; ++k) {
    if (k == HPX_LOCALITY_ID) continue;
    rank = hpx_lco_chan_array_at(channels, k);
    hpx_call(rank, _allreduce, &channels, sizeof(channels), and);
  }
  
  hpx_time_t t1 = hpx_time_now();

  // receive from each rank.
  double *buf;
  double accum = 0;
  for (int k=0; k<ranks; ++k) {
    if (k == HPX_LOCALITY_ID) continue;
    rank = hpx_lco_chan_array_at(channels, HPX_LOCALITY_ID);
    buf = hpx_lco_chan_recv(rank, sizeof(*buf));
    accum += *buf; // reduce
    free(buf);
  }

  printf("root %d allreduce value %f. (ranks = %d)\n", HPX_LOCALITY_ID, accum, ranks);

  for (int k=0; k<ranks; ++k) {
    if (k == HPX_LOCALITY_ID) continue;
    rank = hpx_lco_chan_array_at(channels, k);
    hpx_lco_chan_send(rank, &accum, sizeof(accum), HPX_NULL);
  }

  // wait for receivers to finish.
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  hpx_lco_chan_array_delete(channels, HPX_NULL);

  double elapsed = hpx_time_elapsed_ms(t1);
  printf(" Elapsed: %g\n",elapsed);
  
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
    .gas           = HPX_GAS_PGAS
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

  _main      = HPX_REGISTER_ACTION(_main_action);
  _allreduce = HPX_REGISTER_ACTION(_allreduce_action);
  return hpx_run(_main, NULL, 0);
}
