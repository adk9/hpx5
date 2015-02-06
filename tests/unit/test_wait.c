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

#include "hpx/hpx.h"
#include "tests.h"
#include <stdlib.h>

#define LCOS_PER_LOCALITY 100000
#define WAITERS 4
#define PARTICIPANTS 4

// This tests waiting on lcos.  Best run with many cores. Eg., on cutter:
// mpirun -n 2 -map-by node:PE=16 --tag-output ~/repos/hpx-marcin/tests/unit/hpxtest --hpx-cores=16 --hpx-heapsize=$((1024*1024*1024 * 2)) --hpx-transport=mpi
static HPX_ACTION(_set, const hpx_addr_t * const future) {
  // printf("Setting %zu on %d\n", *future, HPX_LOCALITY_ID);
  hpx_lco_set(*future, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(_wait, const hpx_addr_t * const future) {
  // printf("Waiting on %zu on %d\n", lcos[0], HPX_LOCALITY_ID);
  hpx_lco_wait(*future);
  return HPX_SUCCESS;
}

static HPX_ACTION(_delete, const hpx_addr_t * const lcos) {
  hpx_lco_wait(lcos[2]);
  hpx_lco_delete(lcos[2], HPX_NULL);
  hpx_lco_delete(lcos[0], HPX_NULL);
  hpx_lco_set(lcos[1], 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(_spawn, const hpx_addr_t * const termination_lco) {
  int e;
  for (size_t i = 0; i < LCOS_PER_LOCALITY; ++i) {
    // test futures
    const hpx_addr_t test_futures[3] = {
      hpx_lco_future_new(0),
      *termination_lco,
      hpx_lco_and_new(WAITERS)
    };

    e = hpx_call(HPX_THERE(rand() % HPX_LOCALITIES), _set, HPX_NULL,
                 &test_futures[0], sizeof(hpx_addr_t));
    assert(e == HPX_SUCCESS);

    for(size_t j = 0; j < WAITERS; ++j) {
      e = hpx_call(HPX_THERE(rand() % HPX_LOCALITIES), _wait, test_futures[2],
                   &test_futures[0], sizeof(hpx_addr_t));
      assert(e == HPX_SUCCESS);
    }
    e = hpx_call(HPX_THERE(rand() % HPX_LOCALITIES), _delete, HPX_NULL,
                 test_futures, sizeof(test_futures));
    assert(e == HPX_SUCCESS);

    // test and lco
    const hpx_addr_t test_ands[3] = {
      hpx_lco_and_new(PARTICIPANTS),
      *termination_lco,
      hpx_lco_and_new(WAITERS)
    };

    for(size_t j = 0; j < PARTICIPANTS; ++j) {
      e = hpx_call(HPX_THERE(rand() % HPX_LOCALITIES), _set, HPX_NULL,
                   &test_ands[0], sizeof(hpx_addr_t));
      assert(e == HPX_SUCCESS);
    }

    for(size_t j = 0; j < WAITERS; ++j) {
      e = hpx_call(HPX_THERE(rand() % HPX_LOCALITIES), _wait, test_ands[2],
                   &test_ands[0], sizeof(hpx_addr_t));
      assert(e == HPX_SUCCESS);
    }

    e = hpx_call(HPX_THERE(rand() % HPX_LOCALITIES), _delete, HPX_NULL,
                 test_ands, sizeof(test_ands));
    assert(e == HPX_SUCCESS);
  }
  return HPX_SUCCESS;
}

static HPX_ACTION(test_libhpx_lco_wait, void *UNUSED) {
  printf("Starting the LCO wait test.\n");

  // allocate and start a timer
  const hpx_time_t t1 = hpx_time_now();

  const hpx_addr_t termination_lco = hpx_lco_and_new(2 * LCOS_PER_LOCALITY * HPX_LOCALITIES);
  hpx_bcast(_spawn, HPX_NULL, &termination_lco, sizeof(termination_lco));
  hpx_lco_wait(termination_lco);
  hpx_lco_delete(termination_lco, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}

TEST_MAIN({
  ADD_TEST(test_libhpx_lco_wait);
});
