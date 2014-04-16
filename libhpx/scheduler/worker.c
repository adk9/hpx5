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
/// @file libhpx/scheduler/worker.c
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "hpx/hpx.h"

#include "libsync/sync.h"
#include "libsync/barriers.h"
#include "libsync/deques.h"

#include "libhpx/action.h"
#include "libhpx/builtins.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"                      // used as thread-control block
#include "libhpx/scheduler.h"
#include "libhpx/system.h"
#include "lco.h"
#include "thread.h"
#include "worker.h"


typedef SYNC_ATOMIC(int) atomic_int_t;
typedef SYNC_ATOMIC(atomic_int_t*) atomic_int_atomic_ptr_t;


/// ----------------------------------------------------------------------------
/// Class representing a worker thread's state.
///
/// Worker threads are "object-oriented" insofar as that goes, but each native
/// thread has exactly one, thread-local worker_t structure, so the interface
/// doesn't take a "this" pointer and instead grabs the "self" structure using
/// __thread local storage.
/// ----------------------------------------------------------------------------
/// @{
static __thread struct worker {
  pthread_t          thread;                    // this worker's native thread
  int                    id;                    // this workers's id
  int               core_id;                    // useful for "smart" stealing
  unsigned int         seed;                    // my random seed
  void                  *sp;                    // this worker's native stack
  hpx_parcel_t     *current;                    // current thread
  lco_node_t     *lco_nodes;                    // free LCO nodes
  ustack_t          *stacks;                    // local free stacks
  chase_lev_ws_deque_t work;                    // my work
  atomic_int_t     shutdown;                    // cooperative shutdown flag

  // statistics
  unsigned long     n_spins;
  unsigned long    n_spawns;
  unsigned long    n_steals;
  unsigned long    n_stacks;
} self = {
  .thread    = 0,
  .id        = -1,
  .core_id   = -1,
  .sp        = NULL,
  .current   = NULL,
  .lco_nodes = NULL,
  .stacks    = NULL,
  .work      = SYNC_CHASE_LEV_WS_DEQUE_INIT,
  .shutdown  = 0,
  .n_spins   = 0,
  .n_spawns  = 0,
  .n_steals  = 0,
  .n_stacks  = 0,
};


/// ----------------------------------------------------------------------------
/// The entry function for all of the threads.
/// ----------------------------------------------------------------------------
static void HPX_NORETURN _thread_enter(hpx_parcel_t *parcel) {
  hpx_action_t action = parcel->action;
  hpx_action_handler_t handler = action_lookup(action);
  int status = handler(&parcel->data);
  switch (status) {
   default:
    dbg_error("action produced unhandled error, %i\n", (int)status);
    hpx_shutdown(status);
   case HPX_ERROR:
    dbg_error("action produced error\n");
    hpx_abort();
   case HPX_RESEND:
   case HPX_SUCCESS:
   case HPX_LCO_EXCEPTION:
    hpx_thread_exit(status);
  }
  unreachable();
}


/// ----------------------------------------------------------------------------
/// A thread_transfer() continuation that runs after a worker first starts it's
/// scheduling loop, but before any user defined lightweight threads run.
/// ----------------------------------------------------------------------------
static int _on_start(hpx_parcel_t *to, void *sp, void *env) {
  assert(sp);

  // checkpoint my native stack pointer
  self.sp = sp;
  self.current = to;

  // wait for the rest of the scheduler to catch up to me
  sync_barrier_join(here->sched->barrier, self.id);

  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Create a new lightweight thread based on the parcel.
///
/// The newly created thread is runnable, and can be thread_transfer()ed to in
/// the same way as any other lightweight thread can be.
///
/// @param p - the parcel that is generating this thread.
/// @returns - a new lightweight thread, as defined by the parcel
/// ----------------------------------------------------------------------------
static hpx_parcel_t *_bind(hpx_parcel_t *p) {
  assert(!p->stack);
  ustack_t *stack = self.stacks;
  if (stack) {
    self.stacks = stack->sp;
    thread_init(stack, p, _thread_enter);
  }
  else {
    stack = thread_new(p, _thread_enter);
    ++self.n_stacks;
  }
  p->stack = stack;
  return p;
}


/// ----------------------------------------------------------------------------
/// Steal a lightweight thread during scheduling.
///
/// NB: we can be much smarter about who to steal from and how much to
/// steal. Ultimately though, we're building a distributed runtime though, so
/// SMP work stealing isn't that big a deal.
/// ----------------------------------------------------------------------------
static hpx_parcel_t *_steal(void) {
  int victim_id = rand_r(&self.seed) % here->sched->n_workers;
  if (victim_id == self.id)
    return NULL;

  worker_t *victim = here->sched->workers[victim_id];
  hpx_parcel_t *p = sync_chase_lev_ws_deque_steal(&victim->work);
  if (p)
    ++self.n_steals;

  return p;
}


/// ----------------------------------------------------------------------------
/// Check the network during scheduling.
/// ----------------------------------------------------------------------------
static hpx_parcel_t *_network(void) {
  return network_recv(here->network);
}

static void _free_stack(ustack_t *stack) {
  assert(stack);
  stack->sp = self.stacks;
  self.stacks = stack;
}

/// ----------------------------------------------------------------------------
/// ----------------------------------------------------------------------------
static int _free_parcel(hpx_parcel_t *to, void *sp, void *env) {
  self.current = to;
  hpx_parcel_t *prev = env;
  _free_stack(prev->stack);
  hpx_parcel_release(prev);
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// ----------------------------------------------------------------------------
static int _resend_parcel(hpx_parcel_t *to, void *sp, void *env) {
  self.current = to;
  hpx_parcel_t *prev = env;
  _free_stack(prev->stack);
  hpx_parcel_send(prev);
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// The main scheduling "loop."
///
/// Selects a new lightweight thread to run. If @p fast is set then the
/// algorithm assumes that the calling thread (also a lightweight thread) really
/// wants to transfer quickly---likely because it is holding an LCO's lock and
/// would like to release it.
///
/// Scheduling quickly likely means not trying to perform a steal operation, and
/// not performing any standard maintenance tasks.
///
/// If @p final is non-null, then this indicates that the current thread, which
/// is a lightweight thread, is available for rescheduling, so the algorithm
/// takes that into mind. If the scheduling loop would like to select @p final,
/// but it is NULL, then the scheduler will return a new thread running the
/// HPX_ACTION_NULL action.
///
/// @param  fast - schedule quickly
/// @param final - a final option if the scheduler wants to give up
/// @returns     - a thread to transfer to
/// ----------------------------------------------------------------------------
static hpx_parcel_t *_schedule(bool fast, hpx_parcel_t *final) {
  // if we're supposed to shutdown, then do so
  // NB: leverages non-public knowledge about transfer asm
  atomic_int_t shutdown;
  sync_load(shutdown, &self.shutdown, SYNC_ACQUIRE);
  if (shutdown) {
    void **temp = &self.sp;
    thread_transfer((hpx_parcel_t*)&temp, _free_parcel, self.current);
  }

  // if there are ready threads, select the next one
  hpx_parcel_t *p = sync_chase_lev_ws_deque_pop(&self.work);
  if (p)
    goto exit;

  // no ready threads try to get some work from the network, if we're not in a
  // hurry
  if (!fast)
    if ((p = _network()))
      goto exit;

  // try to steal some work, if we're not in a hurry
  if (!fast)
    if ((p = _steal()))
      goto exit;

  // as a last resort, return final, or a new empty action
  if (final) {
    p = final;
    goto exit;
  }

  ++self.n_spins;
  p = hpx_parcel_acquire(0);

  // lazy stack binding
 exit:
  assert(p);
  return (p->stack) ? p : _bind(p);
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
void *worker_run(scheduler_t *sched) {
  // initialize my worker structure
  self.thread    = pthread_self();
  self.id        = sync_fadd(&sched->next_id, 1, SYNC_ACQREL);
  self.core_id   = -1; // let linux do this for now
  // self.core_id   = self.id % hpx_get_n_ranks(); // round robin
  self.seed      = self.id;

  // initialize my work structure
  sync_chase_lev_ws_deque_init(&self.work, 64);

  // publish my self structure so other people can steal from me
  here->sched->workers[self.id] = &self;

  // set this thread's affinity
  if (self.core_id > 0)
    system_set_affinity(&self.thread, self.core_id);

  // get a parcel to start the scheduler loop with
  hpx_parcel_t *p = hpx_parcel_acquire(0);
  if (!p) {
    dbg_error("failed to acquire an initial parcel.\n");
    return NULL;
  }

  // bind a stack to transfer to
  _bind(p);
  if (!p) {
    dbg_error("failed to bind an initial stack.\n");
    hpx_parcel_release(p);
    return NULL;
  }

  // transfer to the thread---ordinary shutdown will return here
  int e = thread_transfer(p, _on_start, NULL);
  if (e) {
    dbg_error("shutdown returned error\n");
    return NULL;
  }

  // cleanup the thread's resources---we only return here under normal shutdown
  // termination, otherwise we're canceled and vanish
  while ((p = sync_chase_lev_ws_deque_pop(&self.work))) {
    hpx_parcel_release(p);
  }

  // while (self.free) {
  //   t = self.free;
  //   self.free = self.free->next;
  //   thread_delete(t);
  // }

  // have to join the barrier before deleting my deque because someone might be
  // in the middle of a steal operation
  sync_barrier_join(here->sched->barrier, self.id);
  sync_chase_lev_ws_deque_fini(&self.work);

  printf("node %d, thread %d, spins: %lu, spawns:%lu steals:%lu, stacks:%lu\n",
         hpx_get_my_rank(), self.id, self.n_spins, self.n_spawns,
         self.n_steals, self.n_stacks);

  return NULL;
}

int worker_start(scheduler_t *sched) {
  pthread_t thread;
  int e = pthread_create(&thread, NULL, (void* (*)(void*))worker_run, sched);
  return (e) ? HPX_ERROR : HPX_SUCCESS;
}


void worker_shutdown(worker_t *worker) {
  sync_store(&worker->shutdown, 1, SYNC_RELEASE);
}


void worker_join(worker_t *worker) {
  if (pthread_join(worker->thread, NULL))
    dbg_error("cannot join worker thread %d.\n", worker->id);
}


void worker_cancel(worker_t *worker) {
  if (worker && pthread_cancel(worker->thread))
    dbg_error("cannot cancel worker thread %d.\n", worker->id);
}


/// Spawn a user-level thread.
void scheduler_spawn(hpx_parcel_t *p) {
  assert(self.id >= 0);
  assert(p);
  assert(hpx_addr_try_pin(hpx_parcel_get_target(p), NULL));
  self.n_spawns++;
  sync_chase_lev_ws_deque_push(&self.work, p);  // lazy binding
}


static int _checkpoint_ws_push(hpx_parcel_t *to, void *sp, void *env) {
  self.current = to;
  hpx_parcel_t *prev = env;
  prev->stack->sp = sp;
  sync_chase_lev_ws_deque_push(&self.work, prev);
  return HPX_SUCCESS;
}


/// Yields the current thread.
///
/// This doesn't block the current thread, but gives the scheduler the
/// opportunity to suspend it ans select a different thread to run for a
/// while. It's usually used to avoid busy waiting in user-level threads, when
/// the event we're waiting for isn't an LCO (like user-level lock-based
/// synchronization).
void scheduler_yield(void) {
  // if there's nothing else to do, we can be rescheduled
  hpx_parcel_t *from = self.current;
  hpx_parcel_t *to = _schedule(false, from);
  if (from == to)
    return;

  // transfer to the new thread
  thread_transfer(to, _checkpoint_ws_push, self.current);
}


/// ----------------------------------------------------------------------------
/// A transfer continuation that pushes the previous thread onto a an lco
/// queue.
/// ----------------------------------------------------------------------------
static int _unlock_lco(hpx_parcel_t *to, void *sp, void *env) {
  lco_t *lco = env;
  hpx_parcel_t *prev = self.current;
  self.current = to;
  prev->stack->sp = sp;
  lco_unlock(lco);
  return HPX_SUCCESS;
}


/// Waits for an LCO to be signaled, by using the _unlock_lco() continuation.
///
/// Uses the "fast" form of _schedule(), meaning that schedule will not try very
/// hard to acquire more work if it doesn't have anything else to do right
/// now. This avoids the situation where this thread is holding an LCO's lock
/// much longer than necessary. Furthermore, _schedule() can't try to select the
/// calling thread because it doesn't know about it (it's not in self.ready or
/// self.next, and it's not passed as the @p final parameter to _schedule).
///
/// We reacquire the lock before returning, which maintains the atomicity
/// requirements for LCO actions.
///
/// @precondition The calling thread must hold @p lco's lock.
hpx_status_t scheduler_wait(lco_t *lco) {
  hpx_parcel_t *to = _schedule(true, NULL);

  lco_node_t *n = self.lco_nodes;
  if (n) {
    self.lco_nodes = n->next;
  }
  else {
    n = malloc(sizeof(*n));
  }
  n->next = NULL;
  n->data = self.current;
  lco_enqueue(lco, n);

  thread_transfer(to, _unlock_lco, lco);
  lco_lock(lco);
  return lco_get_status(lco);
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
void scheduler_signal(lco_t *lco, hpx_status_t status) {
  lco_node_t *q = lco_trigger(lco, status);
  while (q) {
    // As soon as we push the thread into the work queue, it could be stolen, so
    // make sure we get it's next first.
    lco_node_t *next = q->next;
    sync_chase_lev_ws_deque_push(&self.work, q->data);
    free(q);
    q = next;
  }
}

/// unified continuation handler
static void HPX_NORETURN _continue(hpx_status_t status,
                                   size_t size, const void *value,
                                   void (*cleanup)(void*), void *env) {
  // if there's a continuation future, then we set it, which could spawn a
  // message if the future isn't local
  hpx_parcel_t *parcel = self.current;
  hpx_addr_t cont = parcel->cont;
  if (likely(!hpx_addr_eq(cont, HPX_NULL)))
    hpx_lco_set_status(cont, value, size, status, HPX_NULL);   // async

  // run the cleanup handler
  if (unlikely(cleanup != NULL))
    cleanup(env);

  hpx_parcel_t *to = _schedule(false, NULL);
  thread_transfer(to, _free_parcel, parcel);
  unreachable();
}


void hpx_thread_continue(size_t size, const void *value) {
  _continue(HPX_SUCCESS, size, value, NULL, NULL);
}


void hpx_thread_continue_cleanup(size_t size, const void *value,
                                 void (*cleanup)(void*), void *env) {
  _continue(HPX_SUCCESS, size, value, cleanup, env);
}


void hpx_thread_exit(int status) {
  if (likely(status == HPX_SUCCESS) || unlikely(status == HPX_LCO_EXCEPTION)) {
    _continue(status, 0, NULL, NULL, NULL);
    unreachable();
  }

  if (status == HPX_RESEND) {
    hpx_parcel_t *parcel = self.current;
    hpx_parcel_t *to = _schedule(false, NULL);
    thread_transfer(to, _resend_parcel, parcel);
    unreachable();
  }

  dbg_error("unexpected status, %d.\n", status);
  hpx_abort();
}


hpx_parcel_t *scheduler_current_parcel(void) {
  return self.current;
}


int hpx_get_my_thread_id(void) {
  return self.id;
}


hpx_addr_t hpx_thread_current_target(void) {
  return self.current->target;
}


hpx_addr_t hpx_thread_current_cont(void) {
  return self.current->cont;
}


int hpx_thread_get_tls_id(void) {
  ustack_t *stack = self.current->stack;
  if (stack->tls_id < 0)
    stack->tls_id = sync_fadd(&here->sched->next_tls_id, 1, SYNC_ACQ_REL);

  return stack->tls_id;
}
