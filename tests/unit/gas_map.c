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

static int _initialize_handler(float *element, float initializer) {
  *element = initializer;
  hpx_thread_continue(element, sizeof(*element));
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_PINNED, _initialize, _initialize_handler,
                  HPX_POINTER, HPX_FLOAT);

static int _multiply_handler(float *multiplicand, float multiplier) {
  *multiplicand *= multiplier;
  hpx_thread_continue(multiplicand, sizeof(*multiplicand));
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_PINNED, _multiply, _multiply_handler,
                  HPX_POINTER, HPX_FLOAT);

static int _verify_handler(float *element, float expected) {
  return !(*element == expected);
}
static HPX_ACTION(HPX_DEFAULT, HPX_PINNED, _verify, _verify_handler,
                  HPX_POINTER, HPX_FLOAT);

static int map_handler(void) {
  uint64_t bsize = 64*sizeof(float);
  int blocks = HPX_LOCALITIES;
  hpx_addr_t array = hpx_gas_alloc_cyclic(blocks, bsize, 0);
  test_assert(array != HPX_NULL);

  hpx_addr_t out_array = hpx_gas_alloc_cyclic(blocks, bsize, 0);
  test_assert(out_array != HPX_NULL);

  uint64_t nelts = (bsize * blocks)/sizeof(float);
  float initializer = 4.0;

  printf("Testing hpx_gas_map...\n");
  
  hpx_addr_t lco = hpx_lco_future_new(0);
  int e = hpx_gas_map(_initialize, nelts, array, sizeof(float),
                      out_array, sizeof(float),
                      bsize, lco, &initializer);

  test_assert(e == HPX_SUCCESS);
  e = hpx_lco_wait(lco);
  test_assert(e == HPX_SUCCESS);
  hpx_lco_delete(lco, HPX_NULL);

  printf("Testing hpx_gas_map_sync...\n");

  float multiplier = 5.0;
  e = hpx_gas_map_sync(_multiply, nelts, array, sizeof(float),
                       out_array, sizeof(float),
                       bsize, &multiplier);
  test_assert(e == HPX_SUCCESS);

  printf("Verifying results...\n");

  float expected = initializer * multiplier;
  hpx_map(_verify, out_array, nelts, sizeof(float), bsize, &expected);
  hpx_gas_free(array, HPX_NULL);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, map, map_handler);

TEST_MAIN({
    ADD_TEST(map, 0);
});
