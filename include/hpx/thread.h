
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
#include "hpx/mem.h"
#include "hpx/ctx.h"
#include "hpx/mctx.h"
#include "hpx/lco.h"
#include "hpx/list.h"
#include "hpx/map.h"

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

#define HPX_THREAD_STATE_UNDEFINED  255
#define HPX_THREAD_STATE_CREATE       0
#define HPX_THREAD_STATE_INIT         1
#define HPX_THREAD_STATE_PENDING      2
#define HPX_THREAD_STATE_EXECUTING    3
#define HPX_THREAD_STATE_YIELD        4
#define HPX_THREAD_STATE_BLOCKED      5
#define HPX_THREAD_STATE_SUSPENDED    6
#define HPX_THREAD_STATE_TERMINATED   7

/*
 --------------------------------------------------------------------
  Thread Options

  Values for a bitmask that contain option flags for HPX threads.

  Normally, thread stacks and machine context switching buffers
  are reused after a thread is terminated.  Setting the thread
  to DETACHED will cause the scheduler to immediately deallocate
  these data structures upon thread termination.

  BOUND threads are pinned to the same kernel thread as their
  parent, as opposed to being evenly distributed evenly across all 
  kernel threads in a round-robin fashion (the default).
 --------------------------------------------------------------------
*/

#define HPX_THREAD_OPT_NONE        0x00
#define HPX_THREAD_OPT_DETACHED    0x01
#define HPX_THREAD_OPT_BOUND       0x02


/*
 --------------------------------------------------------------------
  Thread Data

  ctx                          Pointer to the thread context
  nid                          Node ID
  tid                          Thread ID
  state                        Queuing State
  opts                         Thread option flags
  func                         Function to run in the thread
  args                         Application data for the thread
  stk                          The stack allocated for the thread
  ss                           Size (in bytes) of the thread stack
  kth                          The current kernel thread
  mctx                         The last machine context
  retval                       The thread's return value (if any)
  parent                       The thread's parent thread
  children                     Child threads of this thread
 --------------------------------------------------------------------
*/

/* An HPX function taking a single generic (void*) argument */
typedef (*hpx_func_t)(void *);

typedef struct _hpx_thread_t {
  hpx_context_t          *ctx;
  hpx_node_id_t           nid;
  hpx_thread_id_t         tid;
  hpx_thread_state_t      state;
  uint16_t                opts;
  hpx_func_t              func;
  void                   *args;
  void                   *stk;
  size_t                  ss;
  struct _hpx_kthread_t  *kth;
  hpx_mctx_context_t     *mctx;
  hpx_future_t            retval;
  struct _hpx_thread_t   *parent;
  hpx_list_t              children;
} hpx_thread_t;


/* the next thread ID */
static hpx_thread_id_t __thread_next_id;


/*
 --------------------------------------------------------------------
  Thread Functions
 --------------------------------------------------------------------
*/

hpx_thread_id_t hpx_thread_get_id(hpx_thread_t *);

hpx_thread_t * hpx_thread_create(hpx_context_t *, hpx_func_t, void *);
void _hpx_thread_destroy(hpx_thread_t *);

hpx_thread_state_t hpx_thread_get_state(hpx_thread_t *);

void hpx_thread_join(hpx_thread_t *, void **);
void hpx_thread_exit(void *);
void hpx_thread_yield(void);

hpx_thread_t * hpx_thread_self(void);

uint16_t hpx_thread_get_opt(hpx_thread_t *);
void hpx_thread_set_opt(hpx_thread_t *, uint16_t);


/*
 --------------------------------------------------------------------
  Private Functions
 --------------------------------------------------------------------
*/

void _hpx_thread_terminate(hpx_thread_t *);


/*
 --------------------------------------------------------------------
  Map Functions
 --------------------------------------------------------------------
*/

uint64_t hpx_thread_map_hash(hpx_map_t *, void *);
bool hpx_thread_map_cmp(void *, void *);

#endif


