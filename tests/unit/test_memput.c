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

static uint64_t block[] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31
};

static HPX_ACTION(_memput_verify, void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  bool result = false;
  const size_t BLOCK_ELEMS = sizeof(block) / sizeof(block[0]);
  for (int i = 0; i < BLOCK_ELEMS; ++i)
    result |= (local[i] != block[i]);

  hpx_gas_unpin(target);
  HPX_THREAD_CONTINUE(result);
}

// Test code -- for memput
static HPX_ACTION(test_libhpx_memput, void *UNUSED) {
  printf("Starting the memput test\n");
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peerid = (rank+1)%size;

  hpx_addr_t data = hpx_gas_global_alloc(size, sizeof(block));
  hpx_addr_t remote = hpx_addr_add(data, peerid * sizeof(block), sizeof(block));

  hpx_time_t t1 = hpx_time_now();
  hpx_addr_t localComplete = hpx_lco_future_new(0);
  hpx_addr_t remoteComplete = hpx_lco_future_new(0);
  hpx_gas_memput(remote, block, sizeof(block), localComplete, remoteComplete);
  hpx_lco_wait(localComplete);
  hpx_lco_wait(remoteComplete);
  hpx_lco_delete(localComplete, HPX_NULL);
  hpx_lco_delete(remoteComplete, HPX_NULL);
  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));

  bool output = false;
  int e = hpx_call_sync(remote, _memput_verify,
                        &output, sizeof(output), NULL, 0);
  if (e != HPX_SUCCESS) {
    fprintf(stderr, "hpx_call_sync failed with %d", e);
    exit(EXIT_FAILURE);
  }
  assert_msg(output == false, "gas_memput failed");
  return HPX_SUCCESS;
}

TEST_MAIN({
  ADD_TEST(test_libhpx_memput);
});
