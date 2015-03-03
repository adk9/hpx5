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

static HPX_ACTION(_spawn, void *UNUSED) {
  int spawns = rand()%3;
  if (spawns) {
    for (int i = 0; i < spawns; ++i) {
      hpx_call(HPX_HERE, _spawn, HPX_NULL, 0, NULL);
    }
  }
  return HPX_SUCCESS;
}

static HPX_ACTION(test_libhpx_process, void *UNUSED) {
  printf("Test hpx_lco_process\n");

  hpx_addr_t sync = hpx_lco_future_new(0);
  hpx_addr_t proc = hpx_process_new(sync);
  hpx_process_call(proc, HPX_HERE, _spawn, NULL, 0, HPX_NULL);
  hpx_process_call(proc, HPX_HERE, _spawn, NULL, 0, HPX_NULL);
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);
  hpx_process_delete(proc, HPX_NULL);
  hpx_shutdown(HPX_SUCCESS);
}

TEST_MAIN({
 ADD_TEST(test_libhpx_process);
});
