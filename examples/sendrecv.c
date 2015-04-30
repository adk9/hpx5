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
static hpx_action_t _receiver = 0;


static int _receiver_action(double *args, size_t size) {
  hpx_thread_set_affinity(1);
  return HPX_SUCCESS;
}


static int _main_action(int *args, size_t size) {
  int avg = 10000;

  hpx_thread_set_affinity(0);

  hpx_time_t tick = hpx_time_now();
  printf(" Tick: %g\n", hpx_time_us(tick));

  for (int i = 0, e = args[0]; i < e; ++i) {
    double *buf = malloc(sizeof(double)*counts[i]);
    for (int j=0;j<counts[i];++j)
      buf[j] = j*rand();

    printf("%d, %d: ", i, counts[i]);
    hpx_time_t t1 = hpx_time_now();

    // for completing the entire loop
    hpx_addr_t done = hpx_lco_and_new(avg);

    for (int k=0; k<avg; ++k) {
      hpx_call_async(HPX_HERE, _receiver, done, HPX_NULL, buf,
                     sizeof(double) * counts[i]);

      // do the useless work
      double volatile d = 0.;
      for (int i = 0, e = args[1]; i < e; ++i)
        d += 1./(2.*i+1.);
    }

    hpx_lco_wait(done);
    hpx_lco_delete(done, HPX_NULL);

    double elapsed = hpx_time_elapsed_ms(t1);
    printf("Elapsed: %g\n", elapsed/avg);
    free(buf);
  }

  hpx_shutdown(0);
}


static void usage(FILE *f) {
  fprintf(f, "Usage: sendrecv [options] [LEVELS < 24]\n"
          "\t-w, amount of work\n"
          "\t-h, show help\n");
  hpx_print_help();
  fflush(f);
}


int main(int argc, char *argv[argc]) {

  int args[2] = {24, 10000};
  
  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return -1;
  }

  int opt = 0;
  while ((opt = getopt(argc, argv, "w:h?")) != -1) {
    switch (opt) {
     case 'w':
      args[1] = atoi(optarg);
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

  switch (argc) {
   case 1:
     args[0] = atoi(argv[0]);
     break;
   case 0:
     break;
   default:
    usage(stderr);
    return -1;
  }

  if (args[0] > 24) {
    usage(stderr);
    return -1;
  }

  if (HPX_LOCALITIES != 1 || HPX_THREADS < 2) {
    fprintf(stderr, "This test only runs on 1 locality with at least 2 threads!\n");
    return -1;
  }

  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_POINTER, HPX_SIZE_T);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _receiver, _receiver_action, HPX_POINTER, HPX_SIZE_T);
  return hpx_run(&_main, args, sizeof(args));
}
