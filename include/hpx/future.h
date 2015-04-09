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
#ifndef HPX_FUTURE_H
#define HPX_FUTURE_H

#include "hpx/time.h"

// Specifies state of a future as returned by wait_for and wait_until functions
// of future.
typedef enum {
  // The shared state is ready.
  HPX_FUTURE_STATUS_READY,
  // The shared state did not become ready before specified timeout duration
  // has passed.
  HPX_FUTURE_STATUS_TIMEOUT,
  // The shared state contains a deferred function, so the result will be
  // computed only when explicitly requested.
  HPX_FUTURE_STATUS_DEFERRED
} hpx_future_status;

typedef enum {HPX_UNSET = 0x01, HPX_SET = 0x02} hpx_set_t;

#endif
