
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

#include <stdarg.h>
#include <stdint.h>
#include "hpx_mem.h"
#include "hpx_ctx.h"
#include "hpx_mctx.h"

#pragma once
#ifndef LIBHPX_THREAD_H_
#define LIBHPX_THREAD_H_


typedef uint64_t hpx_node_id_t;
typedef uint64_t hpx_thread_id_t;
typedef uint8_t  hpx_thread_state_t;
struct _hpx_kthread_t;


/*
 --------------------------------------------------------------------
  Some Definitions
 --------------------------------------------------------------------
*/

#define HPX_THREAD_STATE_INIT                                      0
#define HPX_THREAD_STATE_PENDING                                   1
#define HPX_THREAD_STATE_EXECUTING                                 2
#define HPX_THREAD_STATE_BLOCKED                                   3
#define HPX_THREAD_STATE_SUSPENDED                                 4
#define HPX_THREAD_STATE_TERMINATED                                4


/*
 --------------------------------------------------------------------
  Thread Data

  nid                          Node ID
  tid                          Thread ID
  state                        Queuing State
 --------------------------------------------------------------------
*/

typedef struct _hpx_thread_t {
  hpx_node_id_t           nid;
  hpx_thread_id_t         tid;
  hpx_thread_state_t      state;
  void *                  func;
  void *                  args;
  void *                  stk;
  size_t                  ss;
  struct _hpx_kthread_t * kth;
  hpx_mctx_context_t *    mctx;
} hpx_thread_t;


/*
 --------------------------------------------------------------------
  Thread Functions
 --------------------------------------------------------------------
*/

hpx_thread_t * hpx_thread_create(hpx_context_t *, void *, void *);
void _hpx_thread_destroy(hpx_thread_t *);

hpx_thread_state_t hpx_thread_get_state(hpx_thread_t *);

void hpx_thread_exit(void **);
void hpx_thread_yield(void);

hpx_thread_t * hpx_thread_self(void);

#endif


