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

#include <stdint.h>                             /* uintptr_t */
#include <inttypes.h>                           /* PRI* */
#include "hpx/error.h"                          /* hpx_error_t */
#include "hpx/thread.h"                         /* hpx_func_t */

/** Forward declarations @{ */
struct hpx_future;
struct hpx_locality;
/** @} */

/**
 * The type that identifies a registered action.
 */
typedef uintptr_t hpx_action_t;

#define HPX_PRIx_hpx_action_t PRIxPTR
#define HPX_PRId_hpx_action_t PRIdPTR
#define HPX_PRIX_hpx_action_t PRIXPTR
#define HPX_PRIu_hpx_action_t PRIuPTR
#define HPX_PRIo_hpx_action_t PRIoPTR

#define HPX_ACTION_NULL 0

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

/**
 * Perform an @p action at a @p location and get a result through a future.
 *
 * @todo 
 *
 * @param[in] location - the location at which to perform the action
 */
hpx_error_t hpx_call(struct hpx_locality *location, hpx_action_t action, void *args, size_t len, struct hpx_future **result);

#endif /* HPX_ACTION_H_ */
