/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Pingong example
  examples/hpx/pingpong.c

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"

#include "debug.h"

static __thread unsigned seed = 0;

static hpx_addr_t rand_rank(void) {
  int r = rand_r(&seed);
  int n = hpx_get_num_ranks();
  return HPX_THERE(r % n);
}

static hpx_action_t send = 0;
static hpx_action_t increment = 0;

static int increment_action(void *args) {
  int n = *(int*)args + 1;
  hpx_thread_continue(sizeof(n), &n);
}


static int send_action(void *args) {
  int n = *(int*)args;
  int i = 0;
  while (i < n) {
    printf("locality: %d, thread: %d, count: %d\n", hpx_get_my_rank(),
           hpx_get_my_thread_id(), i);
    hpx_addr_t future = hpx_lco_future_new(sizeof(n));
    hpx_call(rand_rank(), increment, &i, sizeof(i), future);
    hpx_lco_get(future, &i, sizeof(i));
    hpx_lco_delete(future, HPX_NULL);
  }
  hpx_shutdown(0);
}

static void usage(FILE *f) {
  fprintf(f, "Usage: countdown [options] ROUNDS \n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, show help\n");
}

int main(int argc, char * argv[argc]) {
  hpx_config_t cfg = {
    .cores = 0,
    .threads = 0,
    .stack_bytes = 0
  };

  int debug = NO_RANKS;
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
      debug = ALL_RANKS;
      break;
     case 'd':
      debug = atoi(optarg);
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

  wait_for_debugger(debug);

  if (hpx_init(&cfg)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  send = hpx_register_action("send", send_action);
  increment = hpx_register_action("increment", increment_action);
  return hpx_run(send, &n, sizeof(n));
}
