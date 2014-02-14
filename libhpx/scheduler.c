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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// @file scheduler.c
/// @brief The thread scheduler implementation.
///
/// The thread scheduler is 1/2 of the core of HPX, the other being the
/// network. It is responsible for turning parcels into threads, scheduling
/// threads, synchronizing threads (via LCOs), and balancing workloads.
///
/// The scheduler has two main internal components, 1) a lightweight,
/// stack-based user-level threading subsystem that allows a single scheduler
/// thread to multiplex user level threads, and 2) a global work-stealing
/// scheduling algorithm.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler.h"
#include "ustack.h"
#include "parcel.h"
#include "action.h"
#include "builtins.h"
#include "sync/ms_queue.h"

/// ----------------------------------------------------------------------------
/// Global queue of new parcels to process.
/// ----------------------------------------------------------------------------
static ms_queue_t _new_parcels;

/// ----------------------------------------------------------------------------
/// Thread local scheduler structures.
/// ----------------------------------------------------------------------------
static __thread hpx_parcel_t  *_ready = NULL;
static __thread hpx_parcel_t   *_next = NULL;

static void *_schedule(void);
static int _steal_parcels(void);
static void _useful_wait(void);
static void _thread_entry(hpx_parcel_t *) HPX_NORETURN;

int
scheduler_init(void) {
  sync_ms_queue_init(&_new_parcels);
  return HPX_SUCCESS;
}

int
scheduler_init_thread(void) {
  return HPX_SUCCESS;
}

int
scheduler_startup(hpx_action_t action, const void *args, unsigned size) {
  assert(action);
  hpx_parcel_t *p = hpx_parcel_acquire(size);
  hpx_parcel_set_action(p, action);
  hpx_parcel_set_target(p, HPX_NULL);
  memcpy(hpx_parcel_get_data(p), args, sizeof(size));
  ustack_t *stack = ustack_new(_thread_entry, p);
  return ustack_transfer(stack->sp, ustack_transfer_null);
}

void
scheduler_shutdown(int code) {
  abort();
}

void
scheduler_yield(unsigned n, hpx_addr_t lcos[n]) {
}

void *_schedule(void) {
  hpx_parcel_t *thread = NULL;

 loop:
  thread = _ready;
  if (thread) {
    _ready = _ready->next;
    return thread->stack->sp;
  }

  // epoch transition
  _ready = _next;
  _next = NULL;

  thread = sync_ms_queue_dequeue(&_new_parcels);
  if (thread)
    return ustack_new(_thread_entry, thread)->sp;

  if (_ready)
    goto loop;

  if (_steal_parcels())
    goto loop;

  _useful_wait();
  goto loop;
}

void _thread_entry(hpx_parcel_t *parcel) {
  hpx_action_handler_t f = action_for_key(parcel->action);
  f(parcel->data);
  parcel_release(parcel);
  ustack_transfer(_schedule(), ustack_transfer_delete);
  unreachable();
}

void _useful_wait(void) {
  sleep(1);
}

int _steal_parcels(void) {
  return 0;
}
