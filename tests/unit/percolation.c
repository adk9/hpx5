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
#include <math.h>
#include <hpx/hpx.h>
#include "tests.h"

/// Simple vec add kernel.
const char *_vec_add_kernel =                    "\n" \
  "#pragma OPENCL EXTENSION cl_khr_fp64 : enable  \n" \
  "__kernel void vecAdd(__global double *a,       \n" \
  "                     const unsigned int n_a,   \n" \
  "                     __global double *b,       \n" \
  "                     const unsigned int n_b,   \n" \
  "                     __global double *c,       \n" \
  "                     const unsigned int n_c) { \n" \
  "    int id = get_global_id(0);                 \n" \
  "    if (id < n_a) {                            \n" \
  "      c[id] = a[id] + b[id];                   \n" \
  "    }                                          \n" \
  "}                                              \n" \
  "\n";
static HPX_ACTION(HPX_OPENCL, HPX_MARSHALLED | HPX_VECTORED,
                  _vec_add, _vec_add_kernel);


/// Test percolation.
///
/// This spawns an HPX OpenCL "vector addition" action on one of the
/// discovered devices that supports OpenCL.
///
static int
_test_percolation_handler(void) {
  int n = 100000;
  double *a = calloc(n, sizeof(*a));
  double *b = calloc(n, sizeof(*b));
  double *c = calloc(n, sizeof(*c));

  for (int i = 0; i < n; ++i) {
    a[i] = sinf(i) * sinf(i);
    b[i] = cosf(i) * cosf(i);
  }

  CHECK( hpx_call_sync(HPX_HERE, _vec_add, c, n, a, n, b, n) );

  double sum = 0.0;
  for (int i = 0; i < n; ++i) {
    sum += c[i];
  }
  printf("RESULT: %f\n", sum/n);

  free(a);
  free(b);
  free(c);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _test_percolation, _test_percolation_handler);

TEST_MAIN({
    ADD_TEST(_test_percolation, 0);
  });
