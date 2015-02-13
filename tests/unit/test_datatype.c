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

static struct num {
  int array[10];
} fib = {
  .array = {
    1,
    1,
    2,
    3,
    5,
    8,
    13,
    21,
    34,
    55 }
};

#define lengthof(array) sizeof(array) / sizeof(array[0])

static HPX_ACTION_DECL(_act_array);
static HPX_ACTION_DECL(_act_struct);
static int _action(struct num n) {
  assert(&n.array != &fib.array);
  for (int i = 0, e = lengthof(n.array); i < e; ++i) {
    printf("n[%d] = %d\n", i, n.array[i]);
    assert(n.array[i] == fib.array[i]);
  }
  return HPX_SUCCESS;
}

static HPX_ACTION(test_datatype, void *UNUSED) {
  printf("Test hpx array types\n");
  struct num n = fib;
  hpx_call_sync(HPX_HERE, _act_array, NULL, 0, &n);

  printf("Test hpx struct types\n");
  struct num m = fib;
  hpx_call_sync(HPX_HERE, _act_struct, NULL, 0, &m);

  hpx_shutdown(HPX_SUCCESS);
  return HPX_SUCCESS;
}

int main(int argc, char *argv[]) {
  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "failed to initialize HPX.\n");
    return 1;
  }

  hpx_type_t array_type;
  hpx_array_type_create(&array_type, HPX_INT, 10);
  assert(array_type);
  HPX_REGISTER_TYPED_ACTION(_action, &_act_array, array_type);

  hpx_type_t struct_type;
  hpx_struct_type_create(&struct_type,
                         HPX_INT, HPX_INT, HPX_INT, HPX_INT,
                         HPX_INT, HPX_INT, HPX_INT, HPX_INT,
                         HPX_INT, HPX_INT);
  assert(struct_type);
  HPX_REGISTER_TYPED_ACTION(_action, &_act_struct, struct_type);

  int e = hpx_run(&test_datatype, NULL, 0);
  hpx_type_destroy(array_type);
  return e;
}
