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
#ifndef HPX_ACTION_H_
#define HPX_ACTION_H_

#include "error.h"                              /* hpx_error_t */
#include "thread.h"                             /* hpx_func_t */

/** Forward declarations @{ */
struct hpx_future;
/** @} */

/**
 * The type that identifies a registered action.
 */
typedef uintptr_t hpx_action_t;

/**
 * Register an action with the runtime.
 *
 * @param[in] name - a unique string name for the action
 * @param[in] func - the local function pointer to associate with the action
 */
hpx_action_t hpx_action_register(const char *name, hpx_func_t func);

/**
 * Call after all actions are registered.
 */
void hpx_action_registration_complete(void);

/**
 * Create a local thread to perform the action.
 *
 * @param[in]  action - the action id we want to perform
 * @param[in]  args   - the argument buffer for the action
 * @param[out] future - a future to wait on
 *
 * @returns error code
 */
hpx_error_t hpx_action_invoke(hpx_action_t action, void *args, struct hpx_future **out);

#endif /* HPX_ACTION_H_ */
