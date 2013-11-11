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
#ifndef LIBHPX_ACTION_H_
#define LIBHPX_ACTION_H_

#include <stdint.h>
#include "hpx/thread.h"

typedef uintptr_t hpx_action_t;

/**
 * Register a new action
 */
hpx_action_t hpx_action_register(const char *name, hpx_func_t func);

/**
 * Call after all actions are registered.
 */
void hpx_action_registration_complete(void);

/**
 * Indicate if action registration is complete.
 */
bool hpx_is_action_registration_complete(void);

/**
 * Suspend execution of this thread until registration is complete.
 */
void hpx_waitfor_action_registration_complete(void);

/** 
 *  Lookup actions in the local action table by their name 
 */
hpx_func_t hpx_action_lookup_local(hpx_action_t action);

/**
   Invoke an action
   Note: This is used to invoke an action locally. For remote action
   invocations, see hpx_call(3).
*/
hpx_future_t* hpx_action_invoke(hpx_action_t action, void *args, hpx_thread_t** thp);

#endif /* LIBHPX_ACTION_H_ */
