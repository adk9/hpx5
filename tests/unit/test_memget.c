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

#include <inttypes.h>
#include <stdlib.h>
#include "hpx/hpx.h"
#include "tests.h"

static uint64_t block[] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31
};


static HPX_ACTION(_init_array, void* args) {
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t *local = NULL;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  memcpy(local, args, hpx_thread_current_args_size());
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

#define lengthof(a) sizeof(a) / sizeof(a[0])

// Test code -- for memget
static HPX_ACTION(test_libhpx_memget, void *UNUSED) {
  printf("Starting the memget test\n");
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peerid = (rank+1)%size;

  hpx_addr_t data = hpx_gas_global_alloc(size, sizeof(block));
  hpx_addr_t remote = hpx_addr_add(data, peerid * sizeof(block), sizeof(block));

  hpx_addr_t done = hpx_lco_future_new(sizeof(void*));
  hpx_call(remote, _init_array, done, block, sizeof(block));
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  uint64_t local[lengthof(block)];
  memset(&local, 0xFF, sizeof(local));

  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();
  hpx_addr_t completed = hpx_lco_future_new(0);
  hpx_gas_memget(&local, remote, sizeof(block), completed);
  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);
  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));

  for (int i = 0, e = lengthof(block); i < e; ++i) {
    if (local[i] != block[i]) {
      fprintf(stderr, "failed to get element %d correctly, expected %" PRIu64
              ", got %" PRIu64"\n", i, block[i], local[i]);
      exit(EXIT_FAILURE);
    }
  }
  return HPX_SUCCESS;
}

TEST_MAIN({
  ADD_TEST(test_libhpx_memget);
});
