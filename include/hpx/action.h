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

#include "hpx/thread.h"

#define ACTION_TABLE_SIZE 512

/**
 An HPX action taking a single generic (void*) argument 
*/
typedef struct hpx_action_t {
  char *name;          /* name of the action */
  hpx_func_t action;  /* handle to the function */
} hpx_action_t;

/**
   Register a new action
*/
int hpx_action_register(char *name, hpx_func_t func, hpx_action_t *action);

/** 
    Lookup actions in the local action table by their name 
*/
int hpx_action_lookup_local(char *name, hpx_action_t *action);

/**
   Reverse lookup of actions in the local action table 
*/
int hpx_action_lookup_addr_local(hpx_func_t *func, hpx_action_t *action);

/**
   Invoke an action
   Note: This is used to invoke an action locally. For remote action
   invocations, see hpx_call(3).
*/
hpx_future_t* hpx_action_invoke(hpx_action_t *action, void *args, hpx_thread_t** thp);

#endif /* LIBHPX_ACTION_H_ */
