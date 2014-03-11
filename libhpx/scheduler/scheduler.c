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
#define _GNU_SOURCE /* pthread_setaffinity_np */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// @file libhpx/scheduler/schedule.c
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "sync/sync.h"
#include "sync/barriers.h"
#include "sync/ms_queue.h"
#include "scheduler.h"
#include "locality.h"
#include "thread.h"
#include "lco.h"
#include "builtins.h"
#include "debug.h"

typedef SYNC_ATOMIC(int) atomic_int_t;
typedef SYNC_ATOMIC(atomic_int_t*) atomic_int_atomic_ptr_t;

static __thread int                _id = -1;
static __thread void              *_sp = NULL;
static __thread thread_t        *_free = NULL;
static __thread thread_t       *_ready = NULL;
static __thread thread_t        *_next = NULL;
static __thread atomic_int_t _shutdown = 0;

static int _worker_on_start(void *sp, void *env);
static void *_worker_run(void *id);
static thread_t *_worker_bind(hpx_parcel_t *p);
static thread_t *_worker_steal(void);
static thread_t *_worker_schedule(bool fast, thread_t *final);

static ms_queue_t                        _parcels = {{ 0 }};
static int                             _n_workers = 0;
static pthread_t                        *_workers = NULL;
static sr_barrier_t                     *_barrier = NULL;
static atomic_int_atomic_ptr_t *_shutdown_signals = NULL;

static void _sched_cancel_workers(int i);
static void _sched_join_workers(int i);


int
hpx_get_my_thread_id(void) {
  return _id;
}

int
hpx_get_num_threads(void) {
  return _n_workers;
}

/// Starts the scheduler.
int
scheduler_startup(const hpx_config_t *cfg) {
  int e = HPX_SUCCESS;

  // set the stack size
  thread_set_stack_size(cfg->stack_bytes);

  // initialize the queue
  sync_ms_queue_init(&_parcels);

  // figure out how many worker threads we want to spawn
  _n_workers = cfg->scheduler_threads;
  if (!_n_workers)
    _n_workers = locality_get_n_processors();

  // allocate the array of pthread descriptors
  _workers = calloc(_n_workers, sizeof(_workers[0]));
  if (!_workers) {
    dbg_error("failed to allocate thread table.\n");
    e = errno;
    goto unwind0;
  }

  // allocate the array of shutdown signals
  _shutdown_signals = calloc(_n_workers, sizeof(_shutdown_signals[0]));
  if (!_shutdown_signals) {
    dbg_error("failed to allocate the shutdown signal table.\n");
    e = errno;
    goto unwind1;
  }

  // allocate the global barrier
  _barrier = sr_barrier_new(_n_workers + 1);
  if (!_barrier) {
    dbg_error("failed to allocate the startup barrier.\n");
    e = errno;
    goto unwind2;
  }

  // start all of the worker threads
  int i;
  for (i = 0; i < _n_workers; ++i) {
    void *arg = (void*)(intptr_t)i;
    int e = pthread_create(&_workers[i], NULL, _worker_run, arg);
    if (e) {
      dbg_error("failed to create worker thread #%d.\n", i);
      goto unwind3;
    }

    // set the thread's affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(i % locality_get_n_processors(), &cpuset);
    e = pthread_setaffinity_np(_workers[i], sizeof(cpuset), &cpuset);
    if (e) {
      dbg_error("failed to bind thread affinity for %d", i);
      goto unwind3;
    }
  }

  // release the scheduler threads from their barrier
  sr_barrier_join(_barrier, _n_workers);

  // return success
  return HPX_SUCCESS;

 unwind3:
  _sched_cancel_workers(i);
  _sched_join_workers(i);
  sr_barrier_delete(_barrier);
 unwind2:
  free(_shutdown_signals);
 unwind1:
  free(_workers);
 unwind0:
  return e;
}


/// Set all of the schedule-loop shutdown flags, wait for the workers to cleanup
/// and exit, and then cleanup global scheduler data.
void
scheduler_shutdown(void) {
  // signal all of the shutdown requests
  for (int i = 0; i < _n_workers; ++i) {
    atomic_int_atomic_ptr_t p = sync_load(&_shutdown_signals[i], SYNC_ACQREL);
    sync_store(p, 1, SYNC_RELEASE);
  }

  _sched_join_workers(_n_workers);

  // clean up the parcel queue
  hpx_parcel_t *p = NULL;
  while ((p = sync_ms_queue_dequeue(&_parcels)))
    hpx_parcel_release(p);

  sr_barrier_delete(_barrier);
  free(_shutdown_signals);
  free(_workers);
}


/// Abort the scheduler.
///
/// This will wait for all of the children to cancel, but won't do any cleanup
/// since we have no way to know if they are in async-safe functions that we
/// need during cleanup (e.g., holding the malloc lock).
void
scheduler_abort(void) {
  _sched_cancel_workers(_n_workers);
  _sched_join_workers(_n_workers);
}


/// Spawn a user-level thread.
///
/// Just checks to make sure that the parcel belongs at this locality, and dumps
/// it into the _parcels queue.
void
scheduler_spawn(hpx_parcel_t *p) {
  assert(p);
  assert(hpx_addr_try_pin(hpx_parcel_get_target(p), NULL));
  sync_ms_queue_enqueue(&_parcels, p);
}


/// Yields the current thread.
///
/// This doesn't block the current thread, but gives the scheduler the
/// opportunity to suspend it ans select a different thread to run for a
/// while. It's usually used to avoid busy waiting in user-level threads, when
/// the even we're waiting for isn't an LCO (like user-level lock-based
/// synchronization).
void
scheduler_yield(void) {
  // if there's nothing else to do, we can be rescheduled
  thread_t *from = thread_current();
  thread_t *to = _worker_schedule(false, from);
  if (from == to)
    return;

  // transfer to the new thread, using the thread_checkpoint_push() transfer
  // continuation to checkpoint the current stack and to push the current thread
  // onto the _next epoch list.
  thread_transfer(to->sp, &_next, thread_checkpoint_push);
}


/// Waits for an LCO to be signaled, by using the _transfer_lco() continuation.
///
/// Uses the "fast" form of _schedule(), meaning that schedule will not try very
/// hard to acquire more work if it doesn't have anything else to do right
/// now. This avoids the situation where this thread is holding an LCO's lock
/// much longer than necessary. Furthermore, _schedule() can't try to select the
/// calling thread because it doesn't know about it (it's not in _ready or
/// _next, and it's not passed as the @p final parameter to _schedule).
///
/// We reacquire the lock before returning, which maintains the atomicity
/// requirements for LCO actions.
///
/// @precondition The calling thread must hold @p lco's lock.
void
scheduler_wait(lco_t *lco) {
  thread_t *to = _worker_schedule(true, NULL);
  thread_transfer(to->sp, lco, thread_checkpoint_enqueue);
  lco_lock(lco);
}


/// Signals an LCO.
///
/// This uses lco_trigger() to set the LCO and get it its queued threads
/// back. It then goes through the queue and makes all of the queued threads
/// runnable. It does not release the LCO's lock, that must be done by the
/// caller.
///
/// @todo This does not acknowledge locality in any way. We might want to put
///       the woken threads back up into the worker thread where they were
///       running when they waited.
///
/// @precondition The calling thread must hold @p lco's lock.
void
scheduler_signal(lco_t *lco) {
  thread_t *q = lco_trigger(lco);
  if (q)
    thread_cat(&_next, q);
}


/// Exits a user-level thread.
///
/// This releases the underlying parcel, and deletes the thread structure as the
/// transfer continuation. This will never return, because the current thread is
/// put into the _free threads list and not into a runnable list (_ready, _next,
/// or an lco).
void
scheduler_exit(hpx_parcel_t *parcel) {
  // hpx_parcel_release(parcel);
  thread_t *to = _worker_schedule(false, NULL);
  thread_transfer(to->sp, &_free, thread_exit_push);
  unreachable();
}


/// ----------------------------------------------------------------------------
/// Bind a parcel to a new thread.
/// ----------------------------------------------------------------------------
thread_t *
_worker_bind(hpx_parcel_t *p) {
  thread_t *thread = thread_pop(&_free);
  return (thread) ? thread_init(thread, p) : thread_new(p);
}


/// ----------------------------------------------------------------------------
/// Try and steal some work.
/// ----------------------------------------------------------------------------
thread_t *
_worker_steal(void) {
  hpx_parcel_t *p = sync_ms_queue_dequeue(&_parcels);
  return (p) ? _worker_bind(p) : NULL;
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
/// The @p fast flag tells the scheduler if the caller really needs to complete
/// quickly (probably because it is holding a lock)---this is used by the
/// scheduler_wait() interface that is trying to block on an LCO.
///
/// @param  fast - true, if the caller wants to return quickly
/// @param final - a thread to select if all else fails
/// @returns     - the thread to transfer to
/// ----------------------------------------------------------------------------
thread_t *
_worker_schedule(bool fast, thread_t *final) {
  // if we're supposed to shutdown, then do so
  if (sync_load(&_shutdown, SYNC_ACQUIRE))
    thread_transfer(_sp, &_next, thread_checkpoint_push);

  // if there are ready threads, select the next one
  thread_t *t = thread_pop(&_ready);
  if (t)
    return t;

  // no ready threads, perform an internal epoch transition
  _ready = _next;
  _next = NULL;

  // if the epoch switch has given us some work to do, go do it
  if (_ready)
    return _worker_schedule(fast, final);

  // if we're not in a hurry, try to steal some work
  if (!fast)
    t = _worker_steal();

  // if we stole work, return it
  if (t)
    return t;

  // as a last resort, return final, or a new empty action
  return (final) ? (final) : _worker_bind(hpx_parcel_acquire(0));
}


/// ----------------------------------------------------------------------------
/// The first transfer continuation.
///
/// We checkpoint the native stack pointer, and join the barrier.
/// ----------------------------------------------------------------------------
int
_worker_on_start(void *sp, void *env) {
  assert(sp);
  assert(_barrier);
  _sp = sp;
  sr_barrier_join(_barrier, _id);
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Run a worker thread.
///
/// This is the pthread entry function for a scheduler worker thread. It needs
/// to initialize any thread-local data, and then start up the scheduler. We do
/// this by creating an initial user-level thread and transferring to it.
///
/// Under normal HPX shutdown, we return to the original transfer site and
/// cleanup.
/// ----------------------------------------------------------------------------
void *
_worker_run(void *id) {
  // my id was passed in-place
  _id = (int)(intptr_t)id;

  // expose my shutdown signal
  atomic_int_atomic_ptr_t a = &_shutdown;
  sync_store(&_shutdown_signals[_id], a, SYNC_RELEASE);

  // make myself asynchronously cancellable
  int e = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  if (e) {
    dbg_error("failed to become async cancellable.\n");
    return NULL;
  }

  // get a parcel to start the scheduler loop with
  hpx_parcel_t *p = hpx_parcel_acquire(0);
  if (!p) {
    dbg_error("failed to acquire an initial parcel.\n");
    return NULL;
  }

  // get a thread to transfer to
  thread_t *t = _worker_bind(p);
  if (!t) {
    dbg_error("failed to bind an initial thread.\n");
    hpx_parcel_release(p);
    return NULL;
  }

  // transfer to the thread---ordinary shutdown will return here
  e = thread_transfer(t->sp, NULL, _worker_on_start);
  if (e) {
    dbg_error("shutdown returned error\n");
    return NULL;
  }

  while (_ready) {
    thread_t *t = _ready;
    _ready = _ready->next;
    hpx_parcel_release(t->parcel);
    thread_delete(t);
  }

  while (_next) {
    thread_t *t = _next;
    _next = _next->next;
    hpx_parcel_release(t->parcel);
    thread_delete(t);
  }

  while (_free) {
    thread_t *t = _free;
    _free = _free->next;
    thread_delete(t);
  }

  return NULL;
}

/// ----------------------------------------------------------------------------
/// Loops through the worker threads, and cancels each one.
/// ----------------------------------------------------------------------------
void
_sched_cancel_workers(int i) {
  for (int j = 0; j < i; ++j)
    if (pthread_cancel(_workers[j]))
      dbg_error("cannot cancel worker thread %d.\n", j);
}

/// ----------------------------------------------------------------------------
/// Loops through the worker threads, and joins each one.
/// ----------------------------------------------------------------------------
void
_sched_join_workers(int i) {
  for (int j = 0; j < i; ++j)
    if (pthread_join(_workers[j], NULL))
      dbg_error("cannot join worker thread %d.\n", j);
}

