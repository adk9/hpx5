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

// Size of the data we're transferring.
enum { ELEMENTS = 32 };

static void HPX_NORETURN fail(int i, uint64_t expected, uint64_t actual) {
  fprintf(stderr, "failed to set element %d correctly, "
          "expected %lu, got %lu\n", i, expected, actual);
  exit(EXIT_FAILURE);
}

static HPX_PINNED(_init, uint64_t *local, void* args) {
  for (int i = 0; i < ELEMENTS; ++i) {
    local[i] = i;
  }
  return HPX_SUCCESS;
}

// Test code -- for memget
static HPX_ACTION(gas_memget, void *UNUSED) {
  printf("Starting the memget test\n");
  int peer = (HPX_LOCALITY_ID + 1) % HPX_LOCALITIES;

  // We need a place in registered memory to get() to. We're using the stack in
  // this test. We could also use a registered malloc.
  uint64_t local[ELEMENTS];
  hpx_addr_t data = hpx_gas_global_alloc(HPX_LOCALITIES, sizeof(local));
  hpx_addr_t remote = hpx_addr_add(data, peer * sizeof(local), sizeof(local));
  hpx_call_sync(remote, _init, NULL, 0, NULL, 0);

  // perform the memget
  hpx_time_t t1 = hpx_time_now();
  hpx_gas_memget_sync(local, remote, sizeof(local));
  double elapsed = hpx_time_elapsed_ms(t1);
  printf(" Elapsed: %g\n", elapsed);

  for (int i = 0, e = ELEMENTS; i < e; ++i) {
    if (local[i] != i) {
      fail(i, i, local[i]);
    }
  }

  hpx_gas_free(data, HPX_NULL);
  return HPX_SUCCESS;
}

TEST_MAIN({
  ADD_TEST(gas_memget);
});
