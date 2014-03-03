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
#include <stdio.h>
#include "sync/ms_queue.h"
#include "scheduler.h"
#include "locality.h"
#include "network.h"
#include "thread.h"
#include "entry.h"
#include "lco.h"
#include "builtins.h"

// global queue of new parcels
static ms_queue_t _new_parcels;

static void HPX_CONSTRUCTOR _init_scheduler(void) {
  SYNC_MS_QUEUE_INIT(&_new_parcels);
}

/// Spawns a thread by just pushing the parcel onto the _new_parcels queue.
void
scheduler_spawn(hpx_parcel_t *p) {
  assert(p);
  assert(network_addr_is_local(p->target, NULL));
  SYNC_MS_QUEUE_ENQUEUE(&_new_parcels, p);
}

/// ----------------------------------------------------------------------------
/// The main scheduler loop.
///
/// The purpose of the scheduler loop is to select a new thread to execute. It
/// returns a thread that can be transferred to. It will not block---ultimately
/// it will select @p final if it has to. If @p is NULL, but the scheduler can't
/// find other work to do, then it will return a thread that will run the
/// HPX_ACTION_NULL action.
///
/// The @fast flag tells the scheduler if the caller really needs to complete
/// quickly (probably because it is holding a lock)---this is used by the
/// scheduler_wait() interface that is trying to block on an LCO.
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

void
scheduler_thread_entry(hpx_parcel_t *parcel) {
  hpx_action_handler_t action = locality_action_lookup(parcel->action);
  int status = action(parcel->data);
  if (status != HPX_SUCCESS) {
    fprintf(stderr, "_user_level_thread_entry(): action produced unhandled "
            "error\n");
    hpx_shutdown(status);
  }
  hpx_thread_exit(status, NULL, 0);
  unreachable();
}


/// Freelist for thread-local stacks.
static __thread thread_t *_free_threads = NULL;


/// ----------------------------------------------------------------------------
/// Construct a new thread, either using the freelist, or allocating one.
/// ----------------------------------------------------------------------------
static thread_t *_new(hpx_parcel_t *p) {
  thread_t *thread = _free_threads;
  if (!thread)
    return thread_new(scheduler_thread_entry, p);

  _free_threads = _free_threads->next;
  thread_init(thread, scheduler_thread_entry, p);
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
  hpx_parcel_t *parcel = NULL;
  SYNC_MS_QUEUE_DEQUEUE(&_new_parcels, parcel);
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
/// A transfer continuation that checkpoints the previous stack, and pushes the
/// previous thread onto the thread list designated in @p env.
///
/// @param  sp - the previous stack pointer to checkpoint
/// @param env - a pointer to a list of threads
/// @returns   - success
/// ----------------------------------------------------------------------------
static int _transfer_push(void *sp, void *env) {
  thread_t **list = env;
  thread_t *thread = thread_from_sp(sp);

  // checkpoint previous stack
  thread->sp = sp;

  // push onto the list
  thread->next = *list;
  *list = thread;

  return HPX_SUCCESS;
}


/// Yields the current thread.
///
/// This doesn't block the current thread, but just gives the scheduler the
/// opportunity to do something else for a while. It's usually used to avoid
/// busy waiting in user-level threads, when the even we're waiting for isn't an
/// LCO (like user-level lock-based synchronization).
void
scheduler_yield(void) {
  thread_t *from = thread_current();
  thread_t *to = _schedule(false, from);
  if (from == to)
    return;

  // transfer to the new thread, using the _transfer_push() transfer
  // continuation to checkpoint the current stack and to push the current thread
  // onto the _next epoch list.
  thread_transfer(to->sp, &_next, _transfer_push);
}


/// Exits a user-level thread. This releases the underlying parcel, and deletes
/// the thread structure as the transfer continuation. This will never return,
/// because the current thread is put into the _free_threads ilst and not
/// into a runnable list.
void
scheduler_exit(hpx_parcel_t *parcel) {
  network_release(parcel);
  thread_t *to = _schedule(false, NULL);
  thread_transfer(to->sp, &_free_threads, _transfer_push);
  unreachable();
}


/// ----------------------------------------------------------------------------
/// A transfer continuation that pushes the previous thread onto a an lco list.
///
/// @param  sp - the stack pointer to checkpoint in the current thread
/// @param env - the lco the thread wants to wait on
/// @returns   - success
/// ----------------------------------------------------------------------------
static int _transfer_lco(void *sp, void *env) {
  lco_t *lco = env;
  thread_t *thread = thread_from_sp(sp);

  // checkpoint the thread's stack
  thread->sp = sp;

  // atomically enqueue the thread and release the lco's lock
  lco_enqueue_and_unlock(lco, thread);

  return HPX_SUCCESS;
}


/// Waits for an LCO to be signaled, by using the _transfer_lco() continuation.
///
/// Uses the "fast" form of _schedule(), meaning that schedule will not try very
/// hard to acquire more work if it doesn't have anything else to do right
/// now. This avoids the situation where this thread is holding an LCO's lock
/// much longer than necessary. The scheduler can't choose this thread because
/// it doesn't know about it (it's not in _ready or _next, and it's not passed
/// as the @p final parameter to _schedule).
///
/// We reacquire the lock before returning, which maintains the atomicity
/// requirements for LCO actions.
///
/// @precondition The calling thread must hold @p lco's lock.
void
scheduler_wait(lco_t *lco) {
  thread_t *to = _schedule(true, NULL);
  thread_transfer(to->sp, lco, _transfer_lco);
  lco_lock(lco);
}


/// Signals an LCO.
///
/// This uses lco_trigger() to set the LCO and get it its queued threads
/// back. It then goes through the queue and makes all of the queued threads
/// runnable. It does not release the LCO's lock, that must be done by the
/// caller.
///
/// @precondition The calling thread must hold @p lco's lock.
void
scheduler_signal(lco_t *lco) {
  thread_t *top = lco_trigger(lco);
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

void
scheduler_thread_cancel(void *arg) {
  while (_free_threads) {
    thread_t *t = _free_threads;
    _free_threads = t->next;
    thread_delete(t);
  }
}
