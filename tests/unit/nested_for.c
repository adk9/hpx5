// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

// Goal of this testcase is to test the HPX Nested For Functionality

#include <stdio.h>
#include <stdlib.h>
#include <hpx/hpx.h>
#include "tests.h"

static const int N = 16;
#define BSIZE (N * sizeof(element_t))

static int _initialize_handler(float *element) {
  for (int i = 0; i < N; ++i) {
    element[i] = float(i);
  }
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_PINNED, _initialize, _initialize_handler,
                  HPX_POINTER);

static int nested_for_handler(void) {
  //allocate gas
  hpx_addr_t array = hpx_gas_alloc_cyclic(HPX_LOCALITIES, BSIZE, 0);
  if (NULL == base)
    return HPX_ERROR;
  //fill numbers
  int e = hpx_gas_bcast_sync(_initialize, array, HPX_LOCALITIES, 0, BSIZE);
  if (HPX_SUCCESS != e)
    return e;
  //perform nested for
  hpx_nested_for_sync(array, 0, HPX_LOCALITIES, BSIZE, sizeof(element_t), array);

    hpx_gas_free(array, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(HPX_DEFAULT, 0, nested_for, nested_for_handler);

TEST_MAIN({
    ADD_TEST(nested_for, 0);
  });
