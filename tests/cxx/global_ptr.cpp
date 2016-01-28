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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <iostream>
#include <hpx/hpx++.h>

using namespace std;

hpx_status_t test_addr_arith() {
  int n1 = 10, n2 = 20;
  auto ptr1 = hpx::alloc_cyclic<uint64_t>(n1, 2);
  auto ptr2 = ptr1 + 5;

  auto dist = ptr2 - ptr1;
  // is dist guaranteed to be > 0?
  cout << "dist: " << dist << endl;

  // this should fail?
  auto ptr4 = ptr1[4];

  return HPX_SUCCESS;
}

hpx_status_t test_pin_unpin() {
  int n1 = 10;
  auto ptr1 = hpx::alloc_local<int>(n1);
  hpx::pin_guard<int> guard(ptr1);
  int *p = guard.get();
  for (int i = 0; i != n1; i++) {
    p[i] = i;
  }
  return HPX_SUCCESS;
}

hpx_status_t test_subscript() {
  int n1 = 10;
  auto ptr = hpx::alloc_cyclic<uint64_t>(n1, 1);
  hpx::global_ptr<uint64_t> ptr1 = &ptr[2];

  //   uint64_t val = ptr[2]; // not allowed

  return HPX_SUCCESS;
}

int main(int argc, char* argv[]) {

  int e = hpx::init(&argc, &argv);
  if (e) {
    cerr << "HPX: failed to initialize." << endl;
    return e;
  }

  // TODO use hpx test framework
  test_addr_arith();

  hpx::finalize();
  return 0;
}
