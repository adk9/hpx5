
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

#pragma once
#ifndef LIBHPX_THREAD_H_
#define LIBHPX_THREAD_H_

#include <stdarg.h>
#include <stdint.h>

#include "hpx/mem.h"
#include "hpx/mctx.h"
#include "hpx/lco.h"
#include "hpx/list.h"
#include "hpx/map.h"
#include "hpx/types.h"

typedef uint64_t hpx_node_id_t;
typedef uint64_t hpx_thread_id_t;
typedef uint8_t  hpx_thread_state_t;

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

#define HPX_THREAD_OPT_NONE               0
#define HPX_THREAD_OPT_DETACHED           1
#define HPX_THREAD_OPT_BOUND              2
#define HPX_THREAD_OPT_SERVICE_CORELOCAL  4
#define HPX_THREAD_OPT_SERVICE_COREGLOBAL 8


/* An HPX function taking a single generic (void*) argument */
typedef void (*hpx_func_t)(void *);

/* Predicate functions for _hpx_thread_wait */
typedef bool (*hpx_thread_wait_pred_t)(void *, void *);


/*
 --------------------------------------------------------------------
  Reusable Thread Data

  func                         The thread's execute function
  args                         Arguments to the execute function
  stk                          The thread's call stack
  ss                           The thread's stack size
  mctx                         Machine context switching data
  f_wait                       The thread's waiting future
  kth                          The thread's kernel thread
 --------------------------------------------------------------------
*/

struct hpx_thread_reusable_t {
  void                (*func)(void *);
  void                *args;
  void                *stk;
  size_t               ss;
  hpx_mctx_context_t  *mctx;
  void                *wait;
  hpx_kthread_t       *kth;
};


/*
 --------------------------------------------------------------------
  Thread Data

  ctx                          Pointer to the thread context
  nid                          Node ID
  tid                          Thread ID
  state                        Queuing State
  opts                         Thread option flags
  reuse                        The thread's reusable data area
  f_ret                        The thread's return value (if any)
  parent                       The thread's parent thread
 --------------------------------------------------------------------
*/

struct hpx_thread_t {
  hpx_context_t          *ctx;
  hpx_node_id_t           nid;
  hpx_thread_id_t         tid;
  hpx_thread_state_t      state;
  uint16_t                opts;
  uint8_t                 skip;
  hpx_thread_reusable_t  *reuse;
  hpx_future_t           *f_ret;
  hpx_thread_t           *parent;
  hpx_list_t              children;
};

/*
 --------------------------------------------------------------------
  Thread Functions
 --------------------------------------------------------------------
*/

hpx_thread_id_t hpx_thread_get_id(hpx_thread_t *);

hpx_future_t * hpx_thread_create(hpx_context_t *, uint16_t, void (*)(void*), void *, hpx_thread_t **);
void hpx_thread_destroy(hpx_thread_t *);

hpx_thread_state_t hpx_thread_get_state(hpx_thread_t *);

void hpx_thread_join(hpx_thread_t *, void **);
void hpx_thread_exit(void *);
void hpx_thread_yield(void);
void hpx_thread_yield_skip(uint8_t);
void hpx_thread_wait(hpx_future_t *);

hpx_thread_t *hpx_thread_self(void);

uint16_t hpx_thread_get_opt(hpx_thread_t *);
void hpx_thread_set_opt(hpx_thread_t *, uint16_t);


/*
 --------------------------------------------------------------------
  Private Functions
 --------------------------------------------------------------------
*/

void _hpx_thread_terminate(hpx_thread_t *);
void _hpx_thread_wait(void *, hpx_thread_wait_pred_t, void *);

/*
 --------------------------------------------------------------------
  Map Functions
 --------------------------------------------------------------------
*/

uint64_t hpx_thread_map_hash(hpx_map_t *, void *);
bool hpx_thread_map_cmp(void *, void *);

#endif /* LIBHPX_THREAD_H */
