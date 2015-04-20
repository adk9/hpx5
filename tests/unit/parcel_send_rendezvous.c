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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hpx/hpx.h>
#include <libhpx/locality.h>
#include "tests.h"

static HPX_ACTION(_echo, int *args) {
  hpx_thread_continue(hpx_thread_current_args_size(), args);
}

static HPX_ACTION(parcel_send_rendezvous, void *UNUSED) {
  printf("Testing the hpx parcel send function for large parcels\n");
  unsigned seed = 0;
  size_t eagerlimit = here->config->pwc_parceleagerlimit;
  size_t N = eagerlimit / sizeof(int) + 1;
  for (int i = 1, e = 10; i < e; ++i) {
    size_t scale = i * N;
    size_t size = scale * sizeof(int);

    int *send = malloc(size);
    int *recv = malloc(size);
    for (size_t i = 0, e = scale; i < e; ++i) {
      send[i] = rand_r(&seed);
    }

    int peer = (HPX_LOCALITY_ID + rand_r(&seed)) % HPX_LOCALITIES;
    printf("sending %zu integers (%zu-bytes) to %d\n", scale, size, peer);
    hpx_call_sync(HPX_THERE(peer), _echo, recv, size, send, size);

    for (size_t i = 0, e = scale; i < e; ++i) {
      if (send[i] != recv[i]) {
        fprintf(stderr,
                "data corruption\n"
                "scale: %zu\n"
                "offset: %zu\n"
                "expected: %d\n"
                "actual: %d\n", scale, i, send[i], recv[i]);
        exit(EXIT_FAILURE);
      }
    }

    free(send);
    free(recv);
  }
  return HPX_SUCCESS;
}

TEST_MAIN({
  ADD_TEST(parcel_send_rendezvous);
});
