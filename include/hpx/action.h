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

typedef uintptr_t hpx_action_t;
typedef int (*hpx_action_handler_t)(void *);
extern hpx_action_t HPX_ACTION_NULL;


/// ----------------------------------------------------------------------------
/// returns true if the actions are equal
/// ----------------------------------------------------------------------------
bool hpx_action_eq(const hpx_action_t lhs, const hpx_action_t rhs);


/// ----------------------------------------------------------------------------
/// Should be called by the main native thread only, between the execution of
/// hpx_init() and hpx_run(). Should not be called from an HPX lightweight
/// thread.
///
/// @param   id - a unique string name for the action
/// @param func - the local function pointer to associate with the action
/// @returns    - a key to be used for the action when needed
/// ----------------------------------------------------------------------------
hpx_action_t hpx_register_action(const char *id, hpx_action_handler_t func);


/// ----------------------------------------------------------------------------
/// Simplify the registration interface slightly.
/// ----------------------------------------------------------------------------
#define HPX_STR(l) #l
#define HPX_REGISTER_ACTION(f) \
  hpx_register_action(HPX_STR(_hpx##f), (hpx_action_handler_t)f)


#endif
