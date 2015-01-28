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

/// @file libhpx/system/null.c
/// "Home" location for the HPX_ACTION_NULL action..

#include "hpx/hpx.h"

HPX_INTERRUPT(HPX_ACTION_NULL, void) {
  return HPX_SUCCESS;
}
