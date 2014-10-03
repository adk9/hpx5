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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"

static __thread unsigned seed = 0;

static hpx_addr_t rand_rank(void) {
  int r = rand_r(&seed);
  int n = hpx_get_num_ranks();
  return HPX_THERE(r % n);
}

static hpx_action_t send = 0;

static int send_action(void *args) {
  int n = *(int*)args;
  printf("locality: %d, thread: %d, count: %d\n", hpx_get_my_rank(),
         hpx_get_my_thread_id(), n);

  if (n-- <= 0) {
    printf("terminating.\n");
    hpx_shutdown(0);
  }

  hpx_parcel_t *p = hpx_parcel_acquire(NULL,sizeof(int));
  hpx_parcel_set_target(p, rand_rank());
  hpx_parcel_set_action(p, send);
  hpx_parcel_set_data(p, &n, sizeof(n));
  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}

static void usage(FILE *f) {
  fprintf(f, "Usage: countdown [options] ROUNDS \n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-T, select a transport by number (see hpx_config.h)\n"
          "\t-l, log level (-1 for all)\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, show help\n");
}

int main(int argc, char * argv[argc]) {
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:T:l:d:Dh")) != -1) {
    switch (opt) {
     case 'c':
      cfg.cores = atoi(optarg);
      break;
     case 'l':
      cfg.log_level = atoi(optarg);
      break;\
     case 't':
      cfg.threads = atoi(optarg);
      break;
     case 'T':
      cfg.transport = atoi(optarg);
      assert(0 <= cfg.transport && cfg.transport < HPX_TRANSPORT_MAX);
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

  argc -= optind;
  argv += optind;

  int n = 0;
  switch (argc) {
   default:
    usage(stderr);
    return -1;
   case (1):
    n = atoi(argv[0]);
    break;
  }

  if (hpx_init(&cfg)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  send = hpx_register_action("send", send_action);
  return hpx_run(send, &n, sizeof(n));
}
