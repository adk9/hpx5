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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "hpx/hpx.h"
#include "tests.h"

typedef float real4;
typedef double real8;
typedef long double real10;
typedef real8 real_t;
typedef int int_t;

static hpx_type_t index_type;

typedef struct index_t {
  int_t x;
  int_t y;
  int_t z;
} index_t;

static hpx_action_t _create_source_box;
static hpx_action_t _partition_sources;

static HPX_ACTION_DECL(_test_array);
static HPX_ACTION_DECL(_test_struct);

int_t create_source_box(index_t index) {
  printf("create_source_box: index is %d %d %d\n", index.x, index.y, index.z);
  hpx_call_cc(HPX_HERE, _partition_sources, NULL, NULL, &index);
  return HPX_SUCCESS;
}

int_t partition_sources(index_t index) {
  printf("partition_sources: index is %d %d %d\n", index.x, index.y, index.z);
  if (index.x != 11 && index.y != 22 && index.z != 33) {
    // fail here.
  }
  return HPX_SUCCESS;
}

HPX_ACTION(_test_index, void *unused) {
  index_t temp = (index_t) {11, 22, 33};
  hpx_addr_t sync = hpx_lco_future_new(0);
  hpx_call(HPX_HERE, _create_source_box, sync, &temp);
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);
  return HPX_SUCCESS;
}

#define lengthof(array) sizeof(array) / sizeof(array[0])

static const struct num {
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

static int _test_fib(struct num n) {
  assert(&n != &fib);
  for (int i = 0, e = lengthof(n.array); i < e; ++i) {
    printf("n[%d] = %d\n", i, n.array[i]);
    assert(n.array[i] == fib.array[i]);
  }
  return HPX_SUCCESS;
}

static const struct point {
  int n;
  double x;
  double y;
  double z;
} p = {
  .n = 999,
  .x = 3.1415,
  .y = 2.7184,
  .z = 0.110001
};

static HPX_ACTION_DECL(_test_point);
static int _test_nxyz(struct point point) {
  assert(p.n == point.n);
  assert(p.x == point.x);
  assert(p.y == point.y);
  assert(p.z == point.z);
  return HPX_SUCCESS;
}

static HPX_ACTION(test_datatype, void *UNUSED) {
  printf("Test hpx array types\n");
  struct num n = fib;
  hpx_call_sync(HPX_HERE, _test_array, NULL, 0, &n);

  printf("Test hpx struct types 1\n");
  struct num m = fib;
  hpx_call_sync(HPX_HERE, _test_struct, NULL, 0, &m);

  printf("Test hpx struct types 2\n");
  struct point point = p;
  hpx_call_sync(HPX_HERE, _test_point, NULL, 0, &point);

  // printf("Test hpx struct types 3\n");
  // hpx_call_sync(HPX_HERE, _test_index, NULL, 0, NULL);
  
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
  HPX_REGISTER_TYPED_ACTION(DEFAULT, _test_fib, &_test_array, array_type);

  hpx_type_t struct_type;
  hpx_struct_type_create(&struct_type, HPX_INT, HPX_INT, HPX_INT, HPX_INT,
                         HPX_INT, HPX_INT, HPX_INT, HPX_INT, HPX_INT, HPX_INT);
  assert(struct_type);
  HPX_REGISTER_TYPED_ACTION(DEFAULT, _test_fib, &_test_struct, struct_type);

  hpx_type_t point_struct_type;
  hpx_struct_type_create(&point_struct_type, HPX_INT, HPX_DOUBLE, HPX_DOUBLE,
                         HPX_DOUBLE);
  assert(point_struct_type);
  HPX_REGISTER_TYPED_ACTION(DEFAULT, _test_nxyz, &_test_point, point_struct_type);


  hpx_struct_type_create(&index_type, HPX_INT, HPX_INT, HPX_INT); 

  HPX_REGISTER_TYPED_ACTION(PINNED, create_source_box, &_create_source_box,
                            index_type);

  HPX_REGISTER_TYPED_ACTION(PINNED, partition_sources, &_partition_sources,
                            index_type); 
  
  int e = hpx_run(&test_datatype, NULL, 0);

  hpx_type_destroy(point_struct_type);
  hpx_type_destroy(struct_type);
  hpx_type_destroy(array_type);
  hpx_type_destroy(index_type);
  return e;
}
