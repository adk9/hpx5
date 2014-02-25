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
#include "thread.h"
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
/// The purpose of the scheduler loop is to select a new thread to execute. It
/// returns a thread that can be transferred to. It will not block---ultimately
/// it will select @p final if it has to. If @p is NULL, but the scheduler can't
/// find other work to do, then it will return an appropriate thread to transfer
/// to.
///
/// The @fast flag tells the scheduler if the caller really needs to complete
/// quickly (probably because it is holding a lock).
///
/// @param  fast - true, if the caller wants to return quickly
/// @param final - a thread to select if all else fails
/// @returns     - the thread to transfer to
/// ----------------------------------------------------------------------------
static thread_t *_schedule(bool fast, thread_t *final);

/// ----------------------------------------------------------------------------
/// Try and steal threads.
///
/// @returns - the number of stolen threads.
/// ----------------------------------------------------------------------------
static int _steal(void) {
  return 0;
}

/// ----------------------------------------------------------------------------
/// All user-level threads start in this entry function.
///
/// This function executes the specified parcel, deals with any continuation
/// data, frees the parcel, schedules the next thread, and then transfers to the
/// next thread while freeing the current thread.
/// ----------------------------------------------------------------------------
static void HPX_NORETURN _entry(hpx_parcel_t *parcel) {
  hpx_action_handler_t f = action_for_key(parcel->action);
  int status = f(parcel->data);
  ((void)status); /// @todo Do something with this?
  scheduler_exit(parcel);
}

/// ----------------------------------------------------------------------------
/// A thread_transfer() continuation that checkpoints the native stack.
///
/// The native stack isn't suitable for use as a user-level stack, because we
/// can't guarantee that it's large enough or that it has the right
/// alignment. When we first transfer away from it, we record the stack pointer
/// (in _native_stack) so that we can transfer back to it at shutdown, or for
/// any other native-style operations (e.g., during preemptive scheduling if
/// that is implemented).
///
/// This is executed on the original user-level thread.
/// ----------------------------------------------------------------------------
/// @{
static __thread void  *_native_stack = NULL;

static int _post_startup(void *sp, void *env) {
  void **native_stack = env;
  *native_stack = sp;
  return HPX_SUCCESS;
}
/// @}

/// ----------------------------------------------------------------------------
/// A thread_transfer() continuation that returns the exit code.
///
/// This continuation performs any per-scheduler thread cleanup required, like
/// freeing any allocated user-level stacks, etc., and then returns the exit
/// code.
///
/// This is executed on the native thread.
/// ----------------------------------------------------------------------------
/// @{
static int _post_shutdown(void *sp, void *env) {
  int code = (int)(intptr_t)env;
  return code;
}
/// @}

/// ----------------------------------------------------------------------------
/// Freelist for thread-local stacks.
/// ----------------------------------------------------------------------------
static __thread thread_t *_free_threads = NULL;

/// ----------------------------------------------------------------------------
/// Construct a new thread, either using the freelist, or allocating one.
/// ----------------------------------------------------------------------------
static thread_t *_new(hpx_parcel_t *p) {
  thread_t *thread = _free_threads;
  if (!thread)
    return thread_new(_entry, p);

  _free_threads = _free_threads->next;
  thread_init(thread, _entry, p);
  return thread;
}

/// ----------------------------------------------------------------------------
/// Thread local scheduler structures.
/// ----------------------------------------------------------------------------
static __thread thread_t *_ready = NULL;
static __thread thread_t *_next = NULL;

/// ----------------------------------------------------------------------------
/// The main scheduler event.
///
/// @todo - This is a dumb scheduler, we need a much smarter scheduling
///         algorithm for this to be a high performance system. In particular,
///         the epoch mechanism and starting new threads probably needs to take
///         into account various load metrics, as does stealing.
///
/// ----------------------------------------------------------------------------
static thread_t *_schedule(bool fast, thread_t *final) {
  // if there are ready threads, select the next one
  if (_ready) {
    thread_t *next = _ready;
    _ready = _ready->next;
    return next;
  }

  // no ready threads, perform an internal epoch transition
  _ready = _next;
  _next = NULL;

  // try and add a new thread at the epoch boundary
  hpx_parcel_t *parcel = sync_ms_queue_dequeue(&_new_parcels);
  if (parcel)
    return _new(parcel);

  // if the epoch switch has given us some work to do, go do it
  if (_ready)
    return _schedule(fast, final);

  // if we're not in a hurry, and we succeed in stealing work, go do it
  if (!fast && _steal())
    return _schedule(fast, final);

  // if a final option is set, then return it
  if (final)
    return final;

  // create a null action to keep this scheduler alive after the transfer, the
  // default parcel performs the null action in the current locality, and the
  // default entry point _entry will call back to schedule---that's how the
  // scheduler loop is managed
  return _new(hpx_parcel_acquire(0));
}

int
scheduler_init_module(void) {
  sync_ms_queue_init(&_new_parcels);
  return HPX_SUCCESS;
}

void
scheduler_fini_module(void) {
}

// @todo: thread intiailizer and destructor registration
//
// static void
// _fini_thread(void) {
//   while (_free_threads) {
//     thread_t *t = _free_threads;
//     _free_threads = _free_threads->next;
//     thread_delete(t);
//   }
// }

int
scheduler_startup(hpx_action_t action, const void *args, unsigned size) {
  assert(action);
  hpx_parcel_t *p = hpx_parcel_acquire(size);
  hpx_parcel_set_action(p, action);
  hpx_parcel_set_target(p, HPX_NULL);
  if (size)
    memcpy(hpx_parcel_get_data(p), args, size);
  thread_t *thread = thread_new(_entry, p);
  return thread_transfer(thread->sp, &_native_stack, _post_startup);
}

void
scheduler_shutdown(int code) {
  thread_transfer(_native_stack, (void*)(intptr_t)code, _post_shutdown);
  unreachable();
}

void
scheduler_spawn(hpx_parcel_t *p) {
  assert(p);
  sync_ms_queue_enqueue(&_new_parcels, p);
}

void
scheduler_yield(void) {
  thread_t *from = thread_current();
  thread_t *to = _schedule(false, from);
  if (from == to)
    return;
  thread_transfer(to->sp, &_next, thread_transfer_push);
}

void
scheduler_wait(future_t *future) {
  thread_t *to = _schedule(true, NULL);
  thread_transfer(to->sp, &future->waitq, thread_transfer_push_unlock);
  future_lock(future);
}

void
scheduler_exit(hpx_parcel_t *parcel) {
  parcel_release(parcel);
  thread_t *to = _schedule(false, NULL);
  thread_transfer(to->sp, &_free_threads, thread_transfer_push);
  unreachable();
}

void
scheduler_signal(future_t *f, const void *value, int size) {
  thread_t *top = future_set(f, value, size);
  if (!top)
    return;

  thread_t *end = top;
  while (end->next)
    end = end->next;

  end->next = _ready;
  _ready = top;
  return;
}
