
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Thread Function Definitions
  hpx_thread.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of 
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/


#include "hpx_kthread.h"

#pragma once
#ifndef LIBHPX_THREAD_H_
#define LIBHPX_THREAD_H_

#include <stdint.h>
#include "hpx_mem.h"
#include "hpx_ctx.h"


typedef uint64_t hpx_node_id_t;
typedef uint64_t hpx_thread_id_t;
typedef uint8_t  hpx_thread_state_t;


/*
 --------------------------------------------------------------------
  Some Definitions
 --------------------------------------------------------------------
*/

#define HPX_THREAD_STATE_SUSPENDED                                 0
#define HPX_THREAD_STATE_PENDING                                   1
#define HPX_THREAD_STATE_EXECUTING                                 2
#define HPX_THREAD_STATE_BLOCKED                                   3
#define HPX_THREAD_STATE_TERMINATED                                4


/*
 --------------------------------------------------------------------
  Thread Function
 --------------------------------------------------------------------
*/

typedef void *(*hpx_thread_func_t)(void *);


/*
 --------------------------------------------------------------------
  Thread Data

  nid                          Node ID
  tid                          Thread ID
  state                        Queuing State
 --------------------------------------------------------------------
*/

typedef struct {
  hpx_node_id_t       nid;
  hpx_thread_id_t     tid;
  hpx_thread_state_t  state;
  hpx_thread_func_t * func;
  void *              args;
  hpx_kthread_t *     kth;
} hpx_thread_t;


/*
 --------------------------------------------------------------------
  Thread Functions
 --------------------------------------------------------------------
*/

hpx_thread_t * hpx_thread_create(hpx_context_t *, hpx_thread_func_t, void *);
void hpx_thread_destroy(hpx_thread_t *);

hpx_thread_state_t hpx_thread_get_state(hpx_thread_t *);

void hpx_thread_set_state(hpx_thread_t *, hpx_thread_state_t);

#endif


