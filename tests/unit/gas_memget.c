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

#include <stdlib.h>
#include "hpx/hpx.h"
#include "tests.h"

static void HPX_NORETURN fail(int i, uint64_t expected, uint64_t actual) {
  fprintf(stderr, "failed to set element %d correctly, "
          "expected %lu, got %lu\n", i, expected, actual);
  exit(EXIT_FAILURE);
}

static uint64_t block[32] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31
};


static HPX_PINNED(_init_array, uint64_t *local, void* args) {
  size_t n = hpx_thread_current_args_size();
  memcpy(local, args, n);
  return HPX_SUCCESS;
}

// Test code -- for memget
static HPX_ACTION(gas_memget, void *UNUSED) {
  printf("Starting the memget test\n");
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peer = (rank+1)%size;

  hpx_addr_t data = hpx_gas_global_alloc(size, sizeof(block));
  hpx_addr_t remote = hpx_addr_add(data, peer * sizeof(block), sizeof(block));

  hpx_addr_t done = hpx_lco_future_new(sizeof(void*));
  hpx_call(remote, _init_array, done, block, sizeof(block));
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  uint64_t local[32];

  // perform the memget, get its time for fun
  hpx_time_t t1 = hpx_time_now();
  hpx_gas_memget_sync(local, remote, sizeof(local));
  double elapsed = hpx_time_elapsed_ms(t1);
  printf(" Elapsed: %g\n", elapsed);

  for (int i = 0, e = 32; i < e; ++i) {
    if (local[i] != block[i]) {
      fail(i, block[i], local[i]);
    }
  }
  return HPX_SUCCESS;
}

TEST_MAIN({
  ADD_TEST(gas_memget);
});
