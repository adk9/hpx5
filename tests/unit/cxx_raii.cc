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

#include <hpx/hpx.h>
#include "tests.h"

namespace {
  int resource;

  class RAII {
   public:
    RAII() {
      resource = 1;
    }
    ~RAII() {
      resource = 0;
    }
  };

  int _raii_handler(void) {
    RAII raii;
    hpx_thread_exit(HPX_SUCCESS);
  }
  HPX_ACTION(HPX_DEFAULT, 0, _raii, _raii_handler);

  int _verify_handler(void) {
    test_assert_msg(resource == 0, "Destructor was not run correctly\n");
    return HPX_SUCCESS;
  }
  HPX_ACTION(HPX_DEFAULT, 0, _verify, _verify_handler);
}

TEST_MAIN({
    ADD_TEST(_raii, 0);
    ADD_TEST(_verify, 0);
  });
