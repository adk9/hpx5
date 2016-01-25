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

#include <iostream>
#include <hpx/hpx++.h>

namespace {
using namespace hpx;

int test_handler(void) {
  auto f1 = lco::Future<double>::Alloc();
  auto f2 = lco::Future<void>::Alloc();

  hpx_exit(hpx::SUCCESS);
}
HPX_ACTION(HPX_DEFAULT, 0, test, test_handler);
}

int main(int argc, char* argv[]) {

  if (int e = hpx::init(&argc, &argv)) {
    std::cerr << "HPX: failed to initialize." << std::endl;
    return e;
  }

  if (int e = hpx::run(&test)) {
    return e;
  }

  hpx::finalize();
  return 0;
}

