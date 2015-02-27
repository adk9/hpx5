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

#include "hpx/hpx.h"
#include "tests.h"

static HPX_ACTION(_main, void *UNUSED) {
  hpx_shutdown(HPX_SUCCESS);
  return HPX_SUCCESS;
}

int main(int argc, char *argv[]) {
  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "failed to initialize HPX.\n");
    return 1;
  }
  int e = hpx_run(&_main, NULL, 0);
  printf("1 hpx_run returned %d.\n", e);

  return 77;

  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "failed to initialize HPX.\n");
    return 1;
  }
  e = hpx_run(&_main, NULL, 0);
  printf("2 hpx_run returned %d.\n", e);
  return e;
}
