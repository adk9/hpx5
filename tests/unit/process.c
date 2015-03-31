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

static HPX_ACTION(_spawn1, hpx_addr_t *sync) {
  if (sync) {
    hpx_lco_wait(*sync);
  }

  int spawns = rand()%2;
  if (spawns) {
    for (int i = 0; i < spawns; ++i) {
      hpx_call(HPX_HERE, _spawn1, HPX_NULL, 0, NULL);
    }
  }

  return HPX_SUCCESS;
}

static HPX_ACTION(_spawn2, hpx_addr_t *sync) {
  int spawns = rand()%2;
  if (spawns) {
    for (int i = 0; i < spawns; ++i) {
      hpx_call(HPX_HERE, _spawn2, HPX_NULL, 0, NULL);
    }
  }

  if (sync) {
    hpx_lco_set(*sync, 0, NULL, HPX_NULL, HPX_NULL);
  }
  return HPX_SUCCESS;
}

static HPX_ACTION(process, void *UNUSED) {
  printf("Test hpx_lco_process\n");

  hpx_addr_t psync = hpx_lco_future_new(0);
  hpx_addr_t sync = hpx_lco_future_new(0);
  hpx_addr_t proc = hpx_process_new(psync);
  hpx_process_call(proc, HPX_HERE, _spawn1, HPX_NULL, &sync, sizeof(sync));
  hpx_process_call(proc, HPX_HERE, _spawn2, HPX_NULL, &sync, sizeof(sync));
  hpx_lco_wait(psync);
  hpx_lco_delete(psync, HPX_NULL);
  hpx_lco_delete(sync, HPX_NULL);
  hpx_process_delete(proc, HPX_NULL);
  hpx_shutdown(HPX_SUCCESS);
}

TEST_MAIN({
 ADD_TEST(process);
});
