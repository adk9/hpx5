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
#include <hpx/hpx.h>
#include <libhpx/locality.h>
#include <libhpx/boot.h>
#include "tests.h"

static int alltoall_handler(hpx_addr_t done) {
  printf("Entering alltoall_handler at %d\n", HPX_LOCALITY_ID);
  int src[HPX_LOCALITIES][2];
  int dst[HPX_LOCALITIES][2];

  for (int i = 0, e = HPX_LOCALITIES; i < e; ++i) {
    src[i][0] = here->rank;
    src[i][1] = here->rank;
    dst[i][0] = here->rank;
    dst[i][1] = here->rank;
  }

  boot_t *boot = here->boot;
  int e = boot_alltoall(boot, dst, src, sizeof(int), 2*sizeof(int));
  if (e) {
    exit(EXIT_FAILURE);
  }

  for (int i = 0, e = HPX_LOCALITIES; i < e; ++i) {
    if (dst[i][0] != i) {
      exit(EXIT_FAILURE);
    }
    if (dst[i][1] != here->rank) {
      exit(EXIT_FAILURE);
    }
  }

  hpx_call_cc(done, hpx_lco_set_action, NULL, NULL, NULL, 0);
}
static HPX_ACTION_DEF(DEFAULT, alltoall_handler, alltoall, HPX_ADDR);

static HPX_ACTION(libhpx_boot_alltoall, void *UNUSED) {
  printf("Starting libpx_boot_alltoall on %d localities\n", HPX_LOCALITIES);
  hpx_addr_t done = hpx_lco_and_new(HPX_LOCALITIES);
  hpx_bcast_sync(alltoall, &done);
  hpx_lco_wait(done);
  printf("Completed libhpx_boot_alltoall\n");
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL, NULL, 0);
}

TEST_MAIN({
 ADD_TEST(libhpx_boot_alltoall);
});
