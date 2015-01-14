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
#include "config.h"
#endif

#include "hpx/hpx.h"
#include "libhpx/action.h"


/// @file libhpx/system/null.c
/// "Home" location for the HPX_ACTION_NULL action..

hpx_action_t HPX_ACTION_NULL = 0;


/// Global null action doesn't do anything.
static int _null_action(void *args) {
  return HPX_SUCCESS;
}


/// Register the global actions.
static void HPX_CONSTRUCTOR _init(void) {
  LIBHPX_REGISTER_ACTION(_null_action, &HPX_ACTION_NULL);
}

