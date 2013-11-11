/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_PREDEFINED_H_
#define LIBHPX_PREDEFINED_H_

#include "hpx/action.h"
#include "hpx/error.h"

#if 0
struct shutdown_parcel_args {
  unsigned rank;
  hpx_future_t* ret_fut;
};
#endif

hpx_action_t action_set_shutdown_future;      /* set the shutdown_futures for the appropriate rank/locality */
hpx_future_t *shutdown_futures;               /* futures for each rank for shutdown sequence; for use with action_set_shutdown_future */

/**
 * Do any intialization needed by predefined actions.
 */
hpx_error_t init_predefined();

#endif
