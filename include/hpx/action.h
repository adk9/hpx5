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
#ifndef HPX_ACTION_H
#define HPX_ACTION_H

/// @file
/// @brief Types and functions for registering HPX actions.

/// The handle type for HPX actions.
/// This handle can be obtained via hpx_register_action(). It is the used
/// as a parameter type for any HPX function that needs an action (e.g.
/// hpx_run(), hpx_call(), hpx_parcel_set_action()).
typedef uintptr_t hpx_action_t;

/// The type of functions that can be registered with hpx_register_action().
typedef int (*hpx_action_handler_t)(void *);

/// This special action does nothing (i.e. it is a nop).
extern hpx_action_t HPX_ACTION_NULL;

// ----------------------------------------------------------------------------
/// Should be called by the main native thread only, between the execution of
/// hpx_init() and hpx_run(). Should not be called from an HPX lightweight
/// thread.
///
/// @param   id a unique string name for the action
/// @param func the local function pointer to associate with the action
/// @returns    a key to be used for the action when needed
// ----------------------------------------------------------------------------
hpx_action_t hpx_register_action(const char *id, hpx_action_handler_t func);

// ----------------------------------------------------------------------------
// Simplify the registration interface slightly.
// ----------------------------------------------------------------------------

#define HPX_STR(l) #l

/// A convenience macro for registering an HPX action
#define HPX_REGISTER_ACTION(f) \
  hpx_register_action(HPX_STR(_hpx##f), (hpx_action_handler_t)f)


#endif
