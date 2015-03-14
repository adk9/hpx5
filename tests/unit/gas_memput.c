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
#include <hpx/hpx.h>
#include "tests.h"

static void HPX_NORETURN fail(int i, uint64_t expected, uint64_t actual) {
  fprintf(stderr, "failed to set element %d correctly, "
          "expected %lu, got %lu\n", i, expected, actual);
  exit(EXIT_FAILURE);
}

static HPX_PINNED(_verify, uint64_t *local, uint64_t *args) {
  for (int i = 0, e = 32; i < e; ++i) {
    if (local[i] != args[i]) {
      fail(i, args[i], local[i]);
    }
  }
  return HPX_SUCCESS;
}

// Test code -- for memput
static HPX_ACTION(gas_memput, void *UNUSED) {
  printf("Starting the memput test\n");
  fflush(stdout);
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peer = (rank+1)%size;

  // Need a registered memory address for memput. Stack addresses are
  // registered. We can't use static or const here because the compiler will
  // allocate that as .data.
  uint64_t block[32] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31
  };

  // Allocate some global memory to put into.
  hpx_addr_t data = hpx_gas_global_alloc(size, sizeof(block));
  hpx_addr_t remote = hpx_addr_add(data, peer * sizeof(block), sizeof(block));

  hpx_time_t t1 = hpx_time_now();
  hpx_addr_t complete[2] = {
    hpx_lco_future_new(0),
    hpx_lco_future_new(0)
  };
  hpx_gas_memput(remote, block, sizeof(block), complete[0], complete[1]);
  hpx_lco_wait_all(2, complete, NULL);
  hpx_lco_delete(complete[0], HPX_NULL);
  hpx_lco_delete(complete[1], HPX_NULL);
  double elapsed = hpx_time_elapsed_ms(t1);
  printf(" Elapsed: %g\n", elapsed);

  hpx_call_cc(remote, _verify, NULL, NULL, block, sizeof(block));
}

TEST_MAIN({
  ADD_TEST(gas_memput);
});
