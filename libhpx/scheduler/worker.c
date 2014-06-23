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
#include <string.h>

#include "hpx/hpx.h"

#include "libsync/backoff.h"
#include "libsync/barriers.h"
#include "libsync/deques.h"
#include "libsync/queues.h"

#include "hpx/builtins.h"
#include "libhpx/action.h"
#include "libhpx/btt.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"                      // used as thread-control block
#include "libhpx/scheduler.h"
#include "libhpx/stats.h"
#include "libhpx/system.h"
#include "cvar.h"
#include "thread.h"
#include "worker.h"


typedef SYNC_ATOMIC(int) atomic_int_t;
typedef SYNC_ATOMIC(atomic_int_t*) atomic_int_atomic_ptr_t;

typedef two_lock_queue_t _mailbox_t;
typedef two_lock_queue_node_t _envelope_t;

static unsigned int _max(unsigned int lhs, unsigned int rhs) {
  return (lhs > rhs) ? lhs : rhs;
}

static unsigned int _min(unsigned int lhs, unsigned int rhs) {
  return (lhs < rhs) ? lhs : rhs;
}

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
  unsigned int      backoff;                    // the backoff factor
  void                  *sp;                    // this worker's native stack
  hpx_parcel_t     *current;                    // current thread
  _envelope_t    *envelopes;                    // free envelopes
  chase_lev_ws_deque_t work;                    // my work
  _mailbox_t          inbox;                    // envelopes sent to me
  atomic_int_t     shutdown;                    // cooperative shutdown flag
  scheduler_stats_t   stats;                    // scheduler statistics
} self = {
  .thread     = 0,
  .id         = -1,
  .core_id    = -1,
  .seed       = UINT32_MAX,
  .backoff    = 0,
  .sp         = NULL,
  .current    = NULL,
  .envelopes  = NULL,
  .work       = SYNC_CHASE_LEV_WS_DEQUE_INIT,
  .inbox      = {{0}},
  .shutdown   = 0,
  .stats      = SCHEDULER_STATS_INIT
};


static _envelope_t *
_enclose(worker_t *w, hpx_parcel_t *p) {
  _envelope_t *envelope = w->envelopes;
  if (envelope != NULL)
    w->envelopes = w->envelopes->next;
  else
    envelope = malloc(sizeof(*envelope));

  envelope->next  = NULL;
  envelope->value = p;
  return envelope;
}


static hpx_parcel_t *_open(worker_t *w, _envelope_t *mail) {
  mail->next = w->envelopes;
  w->envelopes = mail;
  return mail->value;
}


/// ----------------------------------------------------------------------------
/// The entry function for all of the threads.
/// ----------------------------------------------------------------------------
static void HPX_NORETURN _thread_enter(hpx_parcel_t *parcel) {
  hpx_action_t action = hpx_parcel_get_action(parcel);
  void *args = hpx_parcel_get_data(parcel);
  int status = action_invoke(action, args);
  switch (status) {
   default:
    dbg_error("action produced unhandled error, %i\n", (int)status);
    hpx_shutdown(status);
   case HPX_ERROR:
    dbg_error("action produced error\n");
    hpx_abort();
   case HPX_RESEND:
   case HPX_SUCCESS:
   case HPX_LCO_ERROR:
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
  assert(!parcel_get_stack(p));
  ustack_t *stack = thread_new(p, _thread_enter);
  parcel_set_stack(p, stack);
  return p;
}


/// ----------------------------------------------------------------------------
/// Backoff is called when there is nothing to do.
///
/// This is a place where we could do system maintenance for optimization, etc.,
/// but was is important is that we not try and run any lightweight threads,
/// based on our backoff integer.
///
/// Right now we just use the synchronization library's backoff.
/// ----------------------------------------------------------------------------
static void _backoff(void) {
  hpx_time_t now = hpx_time_now();
  sync_backoff(self.backoff);
  self.stats.backoff += hpx_time_elapsed_ms(now);
  profile_ctr(++self.stats.backoffs);
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
  if (p) {
    self.backoff = _max(1, self.backoff >> 1);
    profile_ctr(++self.stats.steals);
  }
  else {
    self.backoff = _min(here->sched->backoff_max, self.backoff << 1);
  } //

  return p;
}


/// ----------------------------------------------------------------------------
/// Check the network during scheduling.
/// ----------------------------------------------------------------------------
static hpx_parcel_t *_network(void) {
  return network_rx_dequeue(here->network);
}


/// ----------------------------------------------------------------------------
/// Send a mail message to another worker.
/// ----------------------------------------------------------------------------
static void _send_mail(uint32_t id, hpx_parcel_t *p) {
  worker_t    *w = here->sched->workers[id];
  _envelope_t *letter = _enclose(&self, p);
  sync_two_lock_queue_enqueue(&(w->inbox), letter);
}


/// ----------------------------------------------------------------------------
/// Process my mail queue.
/// ----------------------------------------------------------------------------
static void _handle_mail(void) {
  _envelope_t *letter = NULL;
  while ((letter = sync_two_lock_queue_dequeue(&self.inbox)) != NULL) {
    profile_ctr(++self.stats.mail);
    hpx_parcel_t *p = _open(&self, letter);
    sync_chase_lev_ws_deque_push(&self.work, p);
  }
}


/// ----------------------------------------------------------------------------
/// A transfer continuation that frees the current parcel.
///
/// During normal thread termination, the current thread and parcel need to be
/// freed. This can only be done safely once we've transferred away from that
/// thread (otherwise we've freed a stack that we're currently running on). This
/// continuation performs that operation.
/// ----------------------------------------------------------------------------
static int _free_parcel(hpx_parcel_t *to, void *sp, void *env) {
  self.current = to;
  hpx_parcel_t *prev = env;
  ustack_t *stack = parcel_get_stack(prev);
  parcel_set_stack(prev, NULL);
  thread_delete(stack);
  hpx_parcel_release(prev);
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// A transfer continuation that resends the current parcel.
///
/// If a parcel has arrived at the wrong locality because it's target address
/// has been moved, then the application user will want to resend the parcel and
/// terminate the running thread. This transfer continuation performs that
/// operation.
/// ----------------------------------------------------------------------------
static int _resend_parcel(hpx_parcel_t *to, void *sp, void *env) {
  self.current = to;
  hpx_parcel_t *prev = env;
  ustack_t *stack = parcel_get_stack(prev);
  parcel_set_stack(prev, NULL);
  thread_delete(stack);
  hpx_parcel_send_sync(prev);
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
    assert(temp);
    assert(*temp != NULL);
    thread_transfer((hpx_parcel_t*)&temp, _free_parcel, self.current);
  }

  // messages in my inbox are "in limbo" until I receive them---while this call
  // can cause problems with stealing, we currently feel like it is better
  // (heuristically speaking), to maintain work visibility by cleaning out our
  // inbox as fast as possible
  if (!fast)
    _handle_mail();

  // if there are ready parcels, select the next one
  hpx_parcel_t *p = sync_chase_lev_ws_deque_pop(&self.work);

  if (p) {
    assert(!parcel_get_stack(p) || parcel_get_stack(p)->sp);
    goto exit;
  }

  // no ready parcels try to get some work from the network, if we're not in a
  // hurry
  if (!fast)
    if ((p = _network())) {
      assert(!parcel_get_stack(p) || parcel_get_stack(p)->sp);
      goto exit;
    }

  // try to steal some work, if we're not in a hurry
  if (!fast)
    if ((p = _steal()))
      goto exit;

  // statistically-speaking, we consider this condition to be a spin
  profile_ctr(++self.stats.spins);

  // return final, if it was specified
  if (final) {
    p = final;
    goto exit;
  }

  // We didn't find any new work to do, even given our ability to
  // _steal()---this means that the network didn't have anything for us to do,
  // and the victim we randomly selected didn't have anything to do, and we
  // don't have a thread that yielded().
  //
  // If we're not in a hurry, we'd like to backoff so that we don't thrash the
  // network port and other people's scheduler queues.
  if (!fast)
    _backoff();

  p = hpx_parcel_acquire(NULL, 0);

  // lazy stack binding
 exit:
  assert(!parcel_get_stack(p) || parcel_get_stack(p)->sp);
  if (!parcel_get_stack(p))
    return _bind(p);

  return p;
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
  self.id        = sync_fadd(&sched->next_id, 1, SYNC_ACQ_REL);
  /* self.core_id   = -1; // let linux do this for now */
  self.core_id   = self.id % sched->cores;       // round robin
  self.seed      = self.id;
  self.backoff   = 0;

  // initialize my work structures
  sync_chase_lev_ws_deque_init(&self.work, 64);
  sync_two_lock_queue_init(&self.inbox, NULL);

  // publish my self structure so other people can steal from me
  sched->workers[self.id] = &self;

  // set this thread's affinity
  if (self.core_id > 0)
    system_set_affinity(&self.thread, self.core_id);

  // get a parcel to start the scheduler loop with
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, 0);
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
  while ((p = sync_chase_lev_ws_deque_pop(&self.work)))
    hpx_parcel_release(p);

  // print my stats and accumulate into total stats
  {
    char str[16] = {0};
    snprintf(str, 16, "%d", self.id);
    scheduler_print_stats(str, &self.stats);
    scheduler_accum_stats(sched, &self.stats);
  }

  // join the barrier, last one to the barrier prints the totals
  if (sync_barrier_join(sched->barrier, self.id))
    scheduler_print_stats("<totals>", &sched->stats);

  // delete my deque last, as someone might be stealing from it up until the
  // point where everyone has joined the barrier
  sync_chase_lev_ws_deque_fini(&self.work);

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
  assert(hpx_gas_try_pin(hpx_parcel_get_target(p), NULL)); // NULL doesn't pin
  profile_ctr(self.stats.spawns++);
  sync_chase_lev_ws_deque_push(&self.work, p);  // lazy binding
}


static int _checkpoint_ws_push(hpx_parcel_t *to, void *sp, void *env) {
  self.current = to;
  hpx_parcel_t *prev = env;
  parcel_get_stack(prev)->sp = sp;
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

  assert(to);
  assert(parcel_get_stack(to));
  assert(parcel_get_stack(to)->sp);
  // transfer to the new thread
  thread_transfer(to, _checkpoint_ws_push, self.current);
}

void hpx_thread_yield(void) {
  scheduler_yield();
}


/// ----------------------------------------------------------------------------
/// A transfer continuation that unlocks a lock.
/// ----------------------------------------------------------------------------
static int
_unlock(hpx_parcel_t *to, void *sp, void *env)
{
  lockable_ptr_t *lock = env;
  hpx_parcel_t *prev = self.current;
  self.current = to;
  parcel_get_stack(prev)->sp = sp;
  sync_lockable_ptr_unlock(lock);
  return HPX_SUCCESS;
}


hpx_status_t
scheduler_wait(lockable_ptr_t *lock, cvar_t *condition)
{
  // push the current thread onto the condition variable---no lost-update
  // problem here because we're holing the @p lock
  ustack_t *thread = parcel_get_stack(self.current);
  hpx_status_t status = cvar_push_thread(condition, thread);

  // if we successfully pushed, then do a transfer away from this thread,
  // setting the soft affinity so that the thread is woken up in the right
  // place, and releasing the lock across the wait
  if (status == HPX_SUCCESS) {
    thread->affinity = (thread->affinity >= 0) ? thread->affinity : self.id;
    hpx_parcel_t *to = _schedule(true, NULL);
    thread_transfer(to, _unlock, (void*)lock);
    sync_lockable_ptr_lock(lock);
    status = cvar_get_error(condition);
  }
  return status;
}


void
scheduler_signal(cvar_t *cvar)
{
  ustack_t *thread = cvar_pop_thread(cvar);
  if (!thread)
    return;

  if (thread->wait_affinity != self.id)
    _send_mail(thread->wait_affinity, thread->parcel);
  else
    sync_chase_lev_ws_deque_push(&self.work, thread->parcel);
}


void
scheduler_signal_all(struct cvar *cvar)
{
  ustack_t *thread = cvar_pop_all(cvar);
  while (thread) {
    if (thread->wait_affinity != self.id)
      _send_mail(thread->wait_affinity, thread->parcel);
    else
      sync_chase_lev_ws_deque_push(&self.work, thread->parcel);
    thread = thread->next;
  }
}


void
scheduler_signal_error(struct cvar *cvar, hpx_status_t code)
{
  ustack_t *thread = cvar_set_error(cvar, code);
  while (thread) {
    if (thread->wait_affinity != self.id)
      _send_mail(thread->wait_affinity, thread->parcel);
    else
      sync_chase_lev_ws_deque_push(&self.work, thread->parcel);
    thread = thread->next;
  }
}




static void
_call_continuation(hpx_addr_t target, hpx_action_t action,
                   const void *args, size_t len, hpx_status_t status)
{
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(locality_cont_args_t) + len);
    assert(p);
    hpx_parcel_set_target(p, target);
    hpx_parcel_set_action(p, locality_call_continuation);

    // perform the single serialization
    locality_cont_args_t *cargs = (locality_cont_args_t *)hpx_parcel_get_data(p);
    cargs->action = action;
    cargs->status = status;
    memcpy(&cargs->data, args, len);
    hpx_parcel_send(p, HPX_NULL);
}


/// unified continuation handler
static void HPX_NORETURN _continue(hpx_status_t status,
                                   size_t size, const void *value,
                                   void (*cleanup)(void*), void *env) {
  // if there's a continuation future, then we set it, which could spawn a
  // message if the future isn't local
  hpx_parcel_t *parcel = self.current;
  hpx_action_t c_act = hpx_parcel_get_cont_action(parcel);
  hpx_addr_t c_target = hpx_parcel_get_cont_target(parcel);
  if (!hpx_addr_eq(c_target, HPX_NULL) &&
      !hpx_action_eq(c_act, HPX_ACTION_NULL)) {
    if (hpx_action_eq(c_act, hpx_lco_set_action)) {
      hpx_call(c_target, c_act, value, size, HPX_NULL);
    } else {
      _call_continuation(c_target, c_act, value, size, status);
    }
  }

  // run the cleanup handler
  if (cleanup != NULL)
    cleanup(env);

  hpx_parcel_t *to = _schedule(false, NULL);
  assert(to);
  assert(parcel_get_stack(to));
  assert(parcel_get_stack(to)->sp);
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
  if (likely(status == HPX_SUCCESS) || unlikely(status == HPX_LCO_ERROR)) {
    _continue(status, 0, NULL, NULL, NULL);
    unreachable();
  }

  // If we're supposed to be resending, we want to send back an invalidation
  // our estimated owner for the parcel's target address, and then resend the
  // parcel.
  if (status == HPX_RESEND) {
    hpx_parcel_t *parcel = self.current;

    if (here->btt->type == HPX_GAS_AGAS) {
      // Who do we think owns this parcel?
      uint32_t rank = btt_owner(here->btt, parcel->target);
      // Send that as an update to the src of the parcel.
      locality_gas_forward_args_t args = {
        parcel->target,
        rank
      };
      hpx_call(HPX_THERE(parcel->src), locality_gas_forward, &args, sizeof(args),
               HPX_NULL);
    }
    // Get a parcel to transfer to, and transfer using the resend continuation.
    hpx_parcel_t *to = _schedule(false, NULL);
    assert(to);
    assert(parcel_get_stack(to));
    assert(parcel_get_stack(to)->sp);
    thread_transfer(to, _resend_parcel, parcel);
    unreachable();
  }

  dbg_error("unexpected status, %d.\n", status);
  hpx_abort();
}

scheduler_stats_t *thread_get_stats(void) {
  return &self.stats;
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


hpx_addr_t hpx_thread_current_cont_target(void) {
  return self.current->c_target;
}


hpx_action_t hpx_thread_current_cont_action(void) {
  return self.current->c_action;
}


uint32_t hpx_thread_current_args_size(void) {
  return self.current->size;
}


int hpx_thread_get_tls_id(void) {
  ustack_t *stack = parcel_get_stack(self.current);
  if (stack->tls_id < 0)
    stack->tls_id = sync_fadd(&here->sched->next_tls_id, 1, SYNC_ACQ_REL);

  return stack->tls_id;
}


/// ----------------------------------------------------------------------------
/// A thread_transfer() continuation that runs when a thread changes its
/// affinity. This puts the current thread into the mailbox specified in env.
/// ----------------------------------------------------------------------------
static int _move_to(hpx_parcel_t *to, void *sp, void *env) {
  hpx_parcel_t *prev = self.current;
  self.current = to;
  parcel_get_stack(prev)->sp = sp;

  // just send the previous parcel to the targeted worker
  _send_mail((int)(intptr_t)env, prev);
  return HPX_SUCCESS;
}


void hpx_thread_set_affinity(int affinity) {
  assert(affinity >= -1);
  assert(affinity < here->sched->n_workers);
  assert(self.current);
  assert(parcel_get_stack(self.current));
  parcel_get_stack(self.current)->affinity = affinity;
  if (affinity != self.id) {
    hpx_parcel_t *to = _schedule(NULL, false);
    thread_transfer(to, _move_to, (void*)(intptr_t)affinity);
  }
}
