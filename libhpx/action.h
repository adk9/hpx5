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
#ifndef LIBHPX_ACTION_H
#define LIBHPX_ACTION_H

#include "hpx.h"

/// ----------------------------------------------------------------------------
/// @file action.h
///
/// Defines the internal interface to actions.
/// ----------------------------------------------------------------------------

HPX_INTERNAL hpx_action_handler_t action_for_key(hpx_action_t key);

#endif // LIBHPX_ACTION_H
