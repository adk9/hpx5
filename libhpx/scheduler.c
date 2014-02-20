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
#include <stdio.h>
#include <string.h>
#include "scheduler.h"
#include "ustack.h"
#include "parcel.h"
#include "action.h"
#include "future.h"
#include "network.h"
#include "builtins.h"
#include "sync/ms_queue.h"

/// ----------------------------------------------------------------------------
/// Global queue of new parcels to process.
/// ----------------------------------------------------------------------------
static ms_queue_t _new_parcels;

/// ----------------------------------------------------------------------------
/// The main scheduler loop.
///
/// The purpose of the scheduler loop is to select a new stack to execute. It
/// returns a stack pointer appropriate for the first parameter of
/// ustack_transfer(). If there are no stacks to transfer to at this locality,
/// then this will loop, so callers should be careful to put themselves into the
/// _next queue before scheduling, if they'd like to run again. This is an
/// internal function so we're not worried about misuse out of this file.
///
/// @returns - the stack to transfer to
/// ----------------------------------------------------------------------------
static ustack_t *_schedule(void);

/// ----------------------------------------------------------------------------
/// Try and steal threads (i.e., active stacks).
///
/// @returns - the number of stolen threads.
/// ----------------------------------------------------------------------------
static int _steal(void) {
  return 0;
}

/// ----------------------------------------------------------------------------
/// Wait for a while, in the native thread context.
/// ----------------------------------------------------------------------------
static void _wait(int seconds) {
  sleep(seconds);
}

/// ----------------------------------------------------------------------------
/// All user-level threads start in this entry function.
///
/// This function executes the specified parcel, deals with any continuation
/// data, frees the parcel, schedules the next thread, and then transfers to the
/// next thread while freeing the current stack.
/// ----------------------------------------------------------------------------
static void HPX_NORETURN _entry(hpx_parcel_t *parcel) {
  hpx_action_handler_t f = action_for_key(parcel->action);
  int status = f(parcel->data);
  ((void)status); /// @todo Do something with this?
  scheduler_exit(parcel);
}

/// ----------------------------------------------------------------------------
/// A ustack_transfer() continuation that checkpoints the native stack.
///
/// The native stack isn't suitable for use as a user-level stack, because we
/// can't guarantee that it's large enough or that it has the right
/// alignment. When we first transfer away from it, we record the stack pointer
/// (in _native_stack) so that we can transfer back to it at shutdown, or for
/// any other native-style operations (e.g., during preemptive scheduling if
/// that is implemented).
///
/// This is executed on the original user-level stack.
/// ----------------------------------------------------------------------------
/// @{
static __thread void  *_native_stack = NULL;

static int _post_startup(void *sp) {
  _native_stack = sp;
  return HPX_SUCCESS;
}
/// @}

/// ----------------------------------------------------------------------------
/// A ustack_transfer() continuation that returns the exit code.
///
/// When scheduler_shutdown() is called, it records the passed exit code in
/// _exit_code and then transfers back to the native stack. This continuation
/// performs any per-scheduler thread cleanup required, like freeing any
/// allocated user-level stacks, etc., and then returns the _exit_code().
///
/// This is executed on the native stack.
/// ----------------------------------------------------------------------------
/// @{
static __thread int _exit_code = HPX_SUCCESS;

static int _post_shutdown(void *sp) {
  return _exit_code;
}
/// @}

/// ----------------------------------------------------------------------------
/// Thread local scheduler structures.
/// ----------------------------------------------------------------------------
static __thread ustack_t *_ready = NULL;
static __thread ustack_t *_next = NULL;

/// ----------------------------------------------------------------------------
/// The main scheduler loop.
///
/// The purpose of the scheduler loop is to select a new stack to execute. It
/// returns a stack pointer appropriate for the first parameter of
/// ustack_transfer(). If there are no stacks to transfer to at this locality,
/// then this will loop, so callers should be careful to put themselves into the
/// _next queue before scheduling, if they'd like to run again. This is an
/// internal function so we're not worried about misuse out of this file.
/// ----------------------------------------------------------------------------
static ustack_t *_schedule(void) {
  while (1) {
    ustack_t *stack = _ready;
    if (stack) {
      _ready = _ready->next;
      return stack;
    }

    // epoch transition
    _ready = _next;
    _next = NULL;

    hpx_parcel_t *parcel = sync_ms_queue_dequeue(&_new_parcels);
    if (parcel)
      return ustack_new(_entry, parcel);

    if (_ready)
      continue;

    if (_steal())
      continue;

    _wait(1);
  }
}

int
scheduler_init(void) {
  sync_ms_queue_init(&_new_parcels);
  return HPX_SUCCESS;
}

void
scheduler_fini(void) {
}

int
scheduler_init_thread(void) {
  return HPX_SUCCESS;
}

void
scheduler_fini_thread(void) {
}

int
scheduler_startup(hpx_action_t action, const void *args, unsigned size) {
  assert(action);
  hpx_parcel_t *p = hpx_parcel_acquire(size);
  hpx_parcel_set_action(p, action);
  hpx_parcel_set_target(p, HPX_NULL);
  memcpy(hpx_parcel_get_data(p), args, sizeof(size));
  ustack_t *stack = ustack_new(_entry, p);
  return ustack_transfer(stack->sp, _post_startup);
}

void
scheduler_shutdown(int code) {
  _exit_code = code;
  ustack_transfer(_native_stack, _post_shutdown);
  unreachable();
}

void
scheduler_yield(unsigned n, hpx_addr_t lcos[n]) {
  ustack_t *from = ustack_current();

  if (n) {
    for (unsigned i = 0; i < n; ++i) {
      ((void)lcos[i]);
    }
  }
  else {
    from->next = _next;
    _next = from;
  }

  ustack_t *to = _schedule();

  // if we're transferring to the same stack we're on, elide the tranfer
  if (from != to)
    ustack_transfer(to->sp, ustack_transfer_checkpoint);
}

void
scheduler_exit(hpx_parcel_t *parcel) {
  parcel_release(parcel);
  ustack_t *to = _schedule();
  ustack_transfer(to->sp, ustack_transfer_delete);
  unreachable();
}


int
future_signal_action(hpx_future_signal_args_t *args) {
  // figure out which future we need to signal
  ustack_t *thread = ustack_current();
  hpx_parcel_t *parcel = thread->parcel;
  hpx_addr_t addr = parcel->target;
  future_t *future;
  if (!(network_addr_is_local(addr, (void**)&future))) {
    fprintf(stderr, "future_trigger_action() failed to map future address.\n");
    return 1;
  }

  // signal the future, returns the threads that were waiting
  ustack_t *stack = future_signal(future, &args->bytes, args->size);

  while (stack) {
    // pop the stack
    ustack_t *thread = stack;
    stack = thread->next;

    // push the thread onto the ready list
    thread->next = _ready;
    _ready = thread;
  }

  return HPX_SUCCESS;
}
