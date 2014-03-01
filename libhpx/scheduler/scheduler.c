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
/// @file libhpx/scheduler/schedule.c
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdlib.h>
#include "sync/ms_queue.h"
#include "libhpx/scheduler.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"                     // hpx_parcel_t
#include "thread.h"
#include "lco.h"
#include "future.h"
#include "../builtins.h"

static ms_queue_t _new_parcels;                 // global queue of new parcels

/// Spawns a thread by just pushing the parcel onto the new parcels queue.
void
scheduler_spawn(hpx_parcel_t *p) {
  assert(p);
  assert(hpx_addr_to_rank(p->target) == hpx_get_my_rank());
  sync_ms_queue_enqueue(&_new_parcels, p);
}

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
/// This function executes the specified parcel's action on its arguments. If
/// the action returns, it's because either a) the action doesn't need to return
/// a value, or b) the action encountered an error condition and returned an
/// error.
///
/// We deal with the returned status, and then exit the current thread's
/// execution using scheduler_exit().
///
/// @param parcel - the parcel that describes this thread's task
/// ----------------------------------------------------------------------------
static void HPX_NORETURN _user_level_thread_entry(hpx_parcel_t *parcel) {
  hpx_action_handler_t action = locality_action_lookup(parcel->action);
  int status = action(parcel->data);
  ((void)status); /// @todo Do something with this?
  scheduler_exit(parcel);
}


/// Freelist for thread-local stacks.
static __thread thread_t *_free_threads = NULL;


/// ----------------------------------------------------------------------------
/// Construct a new thread, either using the freelist, or allocating one.
/// ----------------------------------------------------------------------------
static thread_t *_new(hpx_parcel_t *p) {
  thread_t *thread = _free_threads;
  if (!thread)
    return thread_new(_user_level_thread_entry, p);

  _free_threads = _free_threads->next;
  thread_init(thread, _user_level_thread_entry, p);
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


/// ----------------------------------------------------------------------------
/// A transfer continuation that pushes the previous thread onto a queue.
/// ----------------------------------------------------------------------------
static int
_transfer_push(void *sp, void *env) {
  thread_t **stack = env;
  thread_t *thread = thread_from_sp(sp);
  thread->sp = sp;
  thread->next = *stack;
  *stack = thread;
  return HPX_SUCCESS;
}


/// Yields the current thread. Used to avoid busy-waiting in user-level
/// threads.
void
scheduler_yield(void) {
  thread_t *from = thread_current();
  thread_t *to = _schedule(false, from);
  if (from == to)
    return;
  thread_transfer(to->sp, &_next, _transfer_push);
}


/// Exits a user-level thread. This releases the underlying parcel, and deletes
/// the thread structure as the transfer continuation. This will never return,
/// because the thread isn't put into a runnable queue anywhere.
void
scheduler_exit(hpx_parcel_t *parcel) {
  network_release(parcel);
  thread_t *to = _schedule(false, NULL);
  thread_transfer(to->sp, &_free_threads, _transfer_push);
  unreachable();
}


/// ----------------------------------------------------------------------------
/// A transfer continuation that pushes the previous thread onto a locked
/// queue.
/// ----------------------------------------------------------------------------
static int
_transfer_wait(void *sp, void *env) {
  lco_t *lco = env;
  thread_t *thread = thread_from_sp(sp);
  thread->sp = sp;
  lco_enqueue_and_unlock(lco, thread);
  // LOCKABLE_PACKED_STACK_PUSH_AND_UNLOCK(stack, thread);
  return HPX_SUCCESS;
}


/// Waits for an LCO to be signaled, by using the _transfer_push_unlock
/// continuation. Reacquires the lock on the LCO before it returns.
void
scheduler_wait(lco_t *lco) {
  thread_t *to = _schedule(true, NULL);
  thread_transfer(to->sp, lco, _transfer_wait);
  lco_lock(lco);
}


/// Signals an LCO.
void
scheduler_signal(lco_t *f) {
  thread_t *top = lco_trigger(f);
  if (!top)
    return;

  // reschedule all of the waiting threads locally
  //
  // @todo - this doesn't take cache locality into account, but that will
  //         require more data to be stored in the thread---namely its last
  //         scheduler thread, and thus needs to be evaluated carefully
  thread_t *end = top;
  while (end->next)
    end = end->next;

  end->next = _ready;
  _ready = top;
  return;
}


/// An array of native scheduler threads.
static int _n_native_threads = 0;
static int *_native_threads = NULL;
static int __thread _native_thread = 0;

int
hpx_get_my_thread_id(void) {
  return _native_thread;
}

/// Initialize the global data for the scheduler.
int
scheduler_init_module(const hpx_config_t *cfg) {
  int e = HPX_SUCCESS;
  if ((e = thread_init_module(cfg->stack_bytes)))
    goto unwind0;
  if ((e = future_init_module()))
    goto unwind1;

  // Prepare global parcel queue
  sync_ms_queue_init(&_new_parcels);

  _n_native_threads = (cfg->scheduler_threads) ? cfg->scheduler_threads :
                         locality_get_n_processors();

  _native_threads = calloc(_n_native_threads, sizeof(*_native_threads));
  if (!_native_threads) {
    e = 1;
    goto unwind2;
  }

  return HPX_SUCCESS;

 unwind2:
  future_fini_module();
 unwind1:
  thread_fini_module();
 unwind0:
  return e;
}


/// Finalizes the global data for the scheduler.
void
scheduler_fini_module(void) {
  future_fini_module();
  thread_fini_module();
}


/// ----------------------------------------------------------------------------
/// The native stack pointer.
///
/// This is checkpointed in scheduler_start() and restored during
/// scheduler_exit().
/// ----------------------------------------------------------------------------
static void *_native_stack = NULL;


/// ----------------------------------------------------------------------------
/// A thread_transfer() continuation that runs after the first transfer.
///
/// The native stack isn't suitable for use as the stack for a user-level
/// thread, because we can't guarantee that it's large enough or that it has the
/// right alignment. When we first transfer away from it, we record the stack
/// pointer (in _native_stack) so that we can transfer back to it at shutdown,
/// or for any other native-style operations (e.g., during preemptive scheduling
/// if that is implemented).
///
/// This can be used to perform any other on-startup events that the initial
/// user-level thread should run, before performing the user-designated action.
/// ----------------------------------------------------------------------------
static int _on_start(void *sp, void *env) {
  void **addr = env;
  *addr = sp;
  return HPX_SUCCESS;
}


/// Called by the main native thread to start execution.
///
/// We simulate a scheduler spawn by allocating a new parcel, and binding a new
/// user-level thread to it. We can then transfer to this new thread. We don't
/// completely abandon the main user-level stack though, we checkpoint it's
/// stack pointer in _native_stack using the _on_start() transfer continuation.
///
/// This stored stack pointer can be used later to transfer back to the native
/// stack, if it becomes necessary. We need to do this differently than other
/// transfers because the native stack isn't guaranteed to be laid out in the
/// same way that a user-level thread's stack is, and we can't just push it onto
/// the _ready or _next queues.
int
scheduler_start(hpx_action_t action, const void *args, unsigned size) {
  assert(action);
  hpx_parcel_t *p = hpx_parcel_acquire(size);
  hpx_parcel_set_action(p, action);
  hpx_parcel_set_target(p, HPX_NULL);
  if (size)
    hpx_parcel_set_data(p, args, size);
  thread_t *thread = thread_new(_user_level_thread_entry, p);
  return thread_transfer(thread->sp, &_native_stack, _on_start);
}


hpx_parcel_t *
scheduler_current_parcel(void) {
  thread_t *thread = thread_current();
  return thread->parcel;
}


hpx_addr_t
scheduler_current_target(void) {
  hpx_parcel_t *p = scheduler_current_parcel();
  return p->target;
}


/// Each scheduler thread needs to be finalized by having any of its allocated
/// thread structures freed. This is a finalization handler registered in
/// scheduler_init_module().
static void
_thread_fini(void) {
  while (_free_threads) {
    thread_t *t = _free_threads;
    _free_threads = _free_threads->next;
    thread_delete(t);
  }
}


/// ----------------------------------------------------------------------------
/// A thread_transfer() continuation that runs after the scheduler stops.
///
/// This continuation performs any per-scheduler thread cleanup required, like
/// freeing any allocated user-level stacks, etc., and then returns the exit
/// code which it extracts from the environment.
///
/// This makes sure to delete the hpx thread that we were previously executing
/// on, so we don't leak its memory.
///
/// This is executed on the native thread's stack.
/// ----------------------------------------------------------------------------
static int _on_stop(void *sp, void *env) {
  thread_t *thread = thread_from_sp(sp);
  network_release(thread->parcel);
  thread_delete(thread);
  _thread_fini();
  int code = (int)(intptr_t)env;
  return code;
}


/// Called by an HPX user-level thread to shutdown the scheduler.
///
/// We use the native stack pointer checkpointed in _on_start() as the transfer
/// target. We use the _on_stop() transfer continuation to pass the exit code
/// through to the call site of scheduler_start().
///
/// The _on_stop() transfer continuation can perform any shutdown actions that
/// are necessary for this thread, that need to run from the context of the
/// native thread stack for whatever reason.
///
/// The
void
scheduler_stop(int code) {
  thread_transfer(_native_stack, (void*)(intptr_t)code, _on_stop);
  unreachable();
}

