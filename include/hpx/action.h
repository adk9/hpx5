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

/// The handle type for HPX actions.  This handle is obtained via
/// hpx_register_action(). It is safe to use this handle only after a
/// call to hpx_finalize_action() or after hpx_run(). It is used as a
/// parameter type for any HPX function that needs an action (e.g.
/// hpx_run(), hpx_call(), hpx_parcel_set_action()).
typedef uint16_t hpx_action_t;

/// The type of functions that can be registered with hpx_register_action().
typedef int (*hpx_action_handler_t)(void *);

/// This special action does nothing (i.e. it is a nop).
extern hpx_action_t HPX_ACTION_NULL;


/// Should be called by the main native thread only, between the execution of
/// hpx_init() and hpx_run(). Should not be called from an HPX lightweight
/// thread.
///
/// @param   a key to be used for the action when needed
/// @param   id a unique string name for the action
/// @param func the local function pointer to associate with the action
/// @returns error code
int hpx_register_action(hpx_action_t *action, const char *id, hpx_action_handler_t func);

#define HPX_INVALID_ACTION_ID UINT16_MAX

#define HPX_STR(l) #l

/// Convenience macros for registering HPX actions
#define HPX_REGISTER_ACTION(act, f) do {                                \
    *act = HPX_INVALID_ACTION_ID;                                       \
    hpx_register_action(act, HPX_STR(_hpx##f), (hpx_action_handler_t)f);\
  } while (0)

#define HPX_REGISTER_PINNED_ACTION(act, f) do {                                \
    *act = HPX_INVALID_ACTION_ID;                                              \
    hpx_register_pinned_action(act, HPX_STR(_hpx##f), (hpx_action_handler_t)f);\
  } while (0)

#define HPX_REGISTER_TASK(act, f) do {                                \
    *act = HPX_INVALID_ACTION_ID;                                     \
    hpx_register_task(act, HPX_STR(_hpx##f), (hpx_action_handler_t)f);\
  } while (0)

#endif
