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

static hpx_action_t _act;
static hpx_type_t _arr;

struct num {
  int array[10];
} n;

static int _action(struct num n) {
  for (int i = 0; i < (sizeof(n.array)/sizeof(n.array[0])); ++i) {
    printf("n[%d] = %d\n", i, n.array[i]);
  }
  return HPX_SUCCESS;
}

static HPX_ACTION(test_datatype, void *UNUSED) {
  struct num n = {.array = {1, 1, 2, 3, 5, 8, 13, 21, 34, 55}};
  printf("Test hpx_array_datatype\n");
  hpx_call_sync(HPX_HERE, _act, NULL, 0, &n);
  hpx_unregister_type(_arr);
  hpx_shutdown(HPX_SUCCESS);
  return HPX_SUCCESS;
} 

int main(int argc, char *argv[]) {
  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "failed to initialize HPX.\n");
    return 1;
  }

  hpx_register_array_type(&_arr, HPX_INT, 10);
  HPX_REGISTER_TYPED_ACTION(_action, &_act, _arr);
  return hpx_run(&test_datatype, NULL, 0);
}
