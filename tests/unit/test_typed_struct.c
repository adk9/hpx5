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

static hpx_addr_t debug;

int_t create_source_box(index_t index) {
  printf("create_source_box: index is %d %d %d\n", index.x, index.y, index.z);
  hpx_call_cc(HPX_HERE, _partition_sources, NULL, NULL, &index);
}

int_t partition_sources(index_t index) {
  int e = HPX_SUCCESS;
  printf("partition_sources: index is %d %d %d\n", index.x, index.y, index.z);
  if (index.x != 11 && index.y != 22 && index.z != 33) {
    e = HPX_ERROR;
  }
  hpx_lco_and_set(debug, HPX_NULL);
  return e;
}

static HPX_ACTION(test_typed_struct, void *unused) {
  printf("Test hpx typed struct.\n");
  index_t index = {11, 22, 33};
  debug = hpx_lco_and_new(1);
  hpx_call(HPX_HERE, _create_source_box, HPX_NULL, &index);
  hpx_lco_wait(debug);
  hpx_lco_delete(debug, HPX_NULL); 
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "failed to initialize HPX.\n");
    return 1;
  }

  hpx_struct_type_create(&index_type, HPX_INT, HPX_INT, HPX_INT); 

  HPX_REGISTER_TYPED_ACTION(PINNED, create_source_box, &_create_source_box,
                            index_type);
  HPX_REGISTER_TYPED_ACTION(PINNED, partition_sources, &_partition_sources,
                            index_type); 
  
  int e = hpx_run(&test_typed_struct, NULL, 0);

  hpx_type_destroy(index_type);
  return e;
}
