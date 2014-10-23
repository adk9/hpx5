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

/// @file libhpx/scheduler/worker.c
/// @brief Implementation of the scheduler worker thread.

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
#include "libhpx/debug.h"
#include "libhpx/gas.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"                      // used as thread-control block
#include "libhpx/scheduler.h"
#include "libhpx/stats.h"
#include "libhpx/system.h"
#include "cvar.h"
#include "thread.h"
#include "termination.h"
#include "worker.h"


typedef volatile int atomic_int_t;
typedef atomic_int_t* volatile atomic_int_atomic_ptr_t;

static unsigned int _max(unsigned int lhs, unsigned int rhs) {
  return (lhs > rhs) ? lhs : rhs;
}

static unsigned int _min(unsigned int lhs, unsigned int rhs) {
  return (lhs < rhs) ? lhs : rhs;
}

/// Class representing a worker thread's state.
///
/// Worker threads are "object-oriented" insofar as that goes, but each native
/// thread has exactly one, thread-local worker_t structure, so the interface
/// doesn't take a "this" pointer and instead grabs the "self" structure using
/// __thread local storage.
///
/// @{
static __thread struct worker {
  pthread_t          thread;                    // this worker's native thread
  int                    id;                    // this workers's id
  int               core_id;                    // useful for "smart" stealing
  unsigned int         seed;                    // my random seed
  unsigned int      backoff;                    // the backoff factor
  void                  *sp;                    // this worker's native stack
  hpx_parcel_t     *current;                    // current thread
  chase_lev_ws_deque_t work;                    // my work
  two_lock_queue_t    inbox;                    // mail sent to me
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
  .work       = SYNC_CHASE_LEV_WS_DEQUE_INIT,
  .inbox      = {{0}},
  .shutdown   = 0,
  .stats      = SCHEDULER_STATS_INIT
};

/// The entry function for all of the lightweight threads.
///
/// This entry function extracts the action and the arguments from the parcel,
/// and then invokes the action on the arguments. If the action returns to this
/// entry function, we dispatch to the correct thread termination handler.
static void HPX_NORETURN _thread_enter(hpx_parcel_t *parcel) {
  const hpx_addr_t target = hpx_parcel_get_target(parcel);
  const uint32_t owner = gas_owner_of(here->gas, target);
  DEBUG_IF (owner != here->rank) {
    dbg_log_sched("received parcel at incorrect rank, resend likely\n");
  }

  hpx_action_t action = hpx_parcel_get_action(parcel);
  void *args = hpx_parcel_get_data(parcel);

  int status = action_invoke(action, args);
  switch (status) {
   default:
    dbg_error("action: produced unhandled error %i.\n", (int)status);
    hpx_shutdown(status);
   case HPX_ERROR:
    dbg_error("action: produced error.\n");
    hpx_abort();
   case HPX_RESEND:
   case HPX_SUCCESS:
   case HPX_LCO_ERROR:
    hpx_thread_exit(status);
  }
  unreachable();
}


/// A thread_transfer() continuation that runs after a worker first starts it's
/// scheduling loop, but before any user defined lightweight threads run.
static int _on_start(hpx_parcel_t *to, void *sp, void *env) {
  // checkpoint my native stack pointer
  self.sp = sp;
  self.current = to;

  // wait for the rest of the scheduler to catch up to me
  sync_barrier_join(here->sched->barrier, self.id);

  return HPX_SUCCESS;
}


/// Create a new lightweight thread based on the parcel.
///
/// The newly created thread is runnable, and can be thread_transfer()ed to in
/// the same way as any other lightweight thread can be.
///
/// @param          p The parcel that is generating this thread.

static void _bind(hpx_parcel_t *p) {
  assert(!parcel_get_stack(p));
#ifdef HPX_PROFILE_STACKS
  unsigned long stacks = sync_fadd(&here->sched->stats.stacks, 1, SYNC_SEQ_CST);
  unsigned long max_stacks = sync_load(&here->sched->stats.max_stacks, SYNC_SEQ_CST);
  while (stacks + 1 > max_stacks) {
    if (sync_cas(&here->sched->stats.max_stacks, max_stacks, stacks, SYNC_SEQ_CST,
                  SYNC_RELAXED))
      break;
    max_stacks = sync_load(&here->sched->stats.max_stacks, SYNC_SEQ_CST);
  }
#endif
  ustack_t *stack = thread_new(p, _thread_enter);
  parcel_set_stack(p, stack);
}


/// Backoff is called when there is nothing to do.
///
/// This is a place where we could do system maintenance for optimization, etc.,
/// but was is important is that we not try and run any lightweight threads,
/// based on our backoff integer.
///
/// Right now we just use the synchronization library's backoff.
static void _backoff(void) {
  hpx_time_t now = hpx_time_now();
  sync_backoff(self.backoff);
  self.stats.backoff += hpx_time_elapsed_ms(now);
  profile_ctr(++self.stats.backoffs);
}


/// Steal a lightweight thread during scheduling.
///
/// NB: we can be much smarter about who to steal from and how much to
/// steal. Ultimately though, we're building a distributed runtime so SMP work
/// stealing isn't that big a deal.
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


/// Check the network during scheduling.
static hpx_parcel_t *_network(void) {
  return network_rx_dequeue(here->network);
}


/// Send a mail message to another worker.
static void _send_mail(uint32_t id, hpx_parcel_t *p) {
  worker_t *w = here->sched->workers[id];
  two_lock_queue_node_t *node = malloc(sizeof(*node));
  node->value = p;
  node->next = NULL;
  sync_two_lock_queue_enqueue_node(&(w->inbox), node);
}


/// Process my mail queue.
static void _handle_mail(void) {
  two_lock_queue_node_t *node = NULL;
  while ((node = sync_two_lock_queue_dequeue_node(&self.inbox)) != NULL) {
    profile_ctr(++self.stats.mail);
    sync_chase_lev_ws_deque_push(&self.work, node->value);
    free(node);
  }
}


/// A transfer continuation that frees the current parcel.
///
/// During normal thread termination, the current thread and parcel need to be
/// freed. This can only be done safely once we've transferred away from that
/// thread (otherwise we've freed a stack that we're currently running on). This
/// continuation performs that operation.
static int _free_parcel(hpx_parcel_t *to, void *sp, void *env) {
  self.current = to;
  hpx_parcel_t *prev = env;
  ustack_t *stack = parcel_get_stack(prev);
  parcel_set_stack(prev, NULL);
  thread_delete(stack);
#ifdef HPX_PROFILE_STACKS
  sync_fadd(&here->sched->stats.stacks, -1, SYNC_SEQ_CST);
#endif
  hpx_parcel_release(prev);
  return HPX_SUCCESS;
}


/// A transfer continuation that resends the current parcel.
///
/// If a parcel has arrived at the wrong locality because its target address has
/// been moved, then the application user will want to resend the parcel and
/// terminate the running thread. This transfer continuation performs that
/// operation.
///
/// The current thread is terminating however, so we release the stack we were
/// running on.
static int _resend_parcel(hpx_parcel_t *to, void *sp, void *env) {
  self.current = to;
  hpx_parcel_t *prev = env;
  ustack_t *stack = parcel_get_stack(prev);
  parcel_set_stack(prev, NULL);
  thread_delete(stack);
  hpx_parcel_send(prev, HPX_NULL);
  return HPX_SUCCESS;
}


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
/// @param      fast Schedule quickly.
/// @param     final A final option if the scheduler wants to give up.
///
/// @returns A thread to transfer to.
static hpx_parcel_t *_schedule(bool fast, hpx_parcel_t *final) {
  // if we're supposed to shutdown, then do so
  // NB: leverages non-public knowledge about transfer asm
  atomic_int_t shutdown = sync_load(&self.shutdown, SYNC_ACQUIRE);
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
    _bind(p);

  assert((uintptr_t)(parcel_get_stack(p)->sp) % 16 == 0);
  return p;
}


/// Run a worker thread.
///
/// This is the pthread entry function for a scheduler worker thread. It needs
/// to initialize any thread-local data, and then start up the scheduler. We do
/// this by creating an initial user-level thread and transferring to it.
///
/// Under normal HPX shutdown, we return to the original transfer site and
/// cleanup.
///
/// @param      sched The scheduler that this thread should be attached to.
void *worker_run(scheduler_t *sched) {
  // join the global address space
  assert(here && here->gas);
  int e = gas_join(here->gas);
  dbg_check(e, "worker: failed to join the global address space.\n");

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
    dbg_error("worker: failed to acquire an initial parcel.\n");
    return NULL;
  }

  // bind a stack to transfer to
  _bind(p);
  assert((uintptr_t)(parcel_get_stack(p)->sp) % 16 == 0);

  if (!p) {
    dbg_error("worker: failed to bind an initial stack.\n");
    hpx_parcel_release(p);
    return NULL;
  }

  // transfer to the thread---ordinary shutdown will return here
  e = thread_transfer(p, _on_start, NULL);
  if (e) {
    dbg_error("worker: shutdown returned error.\n");
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

  // leave the global address space
  assert(here && here->gas);
  gas_leave(here->gas);
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
  if (worker->thread == pthread_self())
    return;

  if (pthread_join(worker->thread, NULL))
    dbg_error("worker: cannot join worker thread %d.\n", worker->id);
}


void worker_cancel(worker_t *worker) {
  if (worker && pthread_cancel(worker->thread))
    dbg_error("worker: cannot cancel worker thread %d.\n", worker->id);
}


/// Spawn a user-level thread.
void scheduler_spawn(hpx_parcel_t *p) {
  assert(self.id >= 0);
  assert(p);
  assert(hpx_gas_try_pin(hpx_parcel_get_target(p), NULL)); // NULL doesn't pin
  profile_ctr(self.stats.spawns++);
  sync_chase_lev_ws_deque_push(&self.work, p);  // lazy binding
}


/// This is the continuation that we use to yield a thread.
///
/// This make flagrantly bad use of the network queue to deal with a number of
/// issues related to yielding threads and workstealing queues.
///
/// 1) We can't put a yielding thread onto our workqueue with the normal push
///    operation, because two threads who are yielding will prevent progress
///    in that scheduler.
/// 2) We can't use our mailbox because we empty that as fast as possible, and
///    we'll wind up with the same problem as above.
/// 3) We don't want to use a separate local queue because we don't want to hide
///    visibility of yielded threads in the case of heavy load.
/// 4) We don't want to push onto the bottom of our workqueue because that
///    requires counted pointers for stealing and yielding and won't solve the
///    problem, a yielding thread can tie up a scheduler.
/// 5) We'd like to use a global queue for yielded threads so that they can be
///    processed in FIFO order by threads that don't have anything else to do.
///
/// We already have the network RX queue, so we'll use it for now. It's checked
/// before stealing though, so two yielders could theoretically inhibit
/// liveness. Ultimately we'll want a separate yielded queue (or queues) to deal
/// with the issue.
static int _checkpoint_network_push(hpx_parcel_t *to, void *sp, void *env) {
  self.current = to;
  hpx_parcel_t *prev = env;
  parcel_get_stack(prev)->sp = sp;
  network_rx_enqueue(here->network, prev);
  return HPX_SUCCESS;
}


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
  thread_transfer(to, _checkpoint_network_push, self.current);
}


void hpx_thread_yield(void) {
  scheduler_yield();
}


/// A transfer continuation that unlocks a lock.
static int _unlock(hpx_parcel_t *to, void *sp, void *env) {
  lockable_ptr_t *lock = env;
  hpx_parcel_t *prev = self.current;
  self.current = to;
  parcel_get_stack(prev)->sp = sp;
  sync_lockable_ptr_unlock(lock);
  return HPX_SUCCESS;
}


hpx_status_t scheduler_wait(lockable_ptr_t *lock, cvar_t *condition) {
  // push the current thread onto the condition variable---no lost-update
  // problem here because we're holing the @p lock
  ustack_t *thread = parcel_get_stack(self.current);
  hpx_status_t status = cvar_push_thread(condition, thread);

  // if we successfully pushed, then do a transfer away from this thread
  if (status == HPX_SUCCESS) {
    hpx_parcel_t *to = _schedule(true, NULL);
    thread_transfer(to, _unlock, (void*)lock);
    sync_lockable_ptr_lock(lock);
    status = cvar_get_error(condition);
  }
  return status;
}

/// Resume a thread.
static inline void _resume(ustack_t *thread) {
  if (thread->affinity >= 0 && thread->affinity != self.id)
    _send_mail(thread->affinity, thread->parcel);
  else
    sync_chase_lev_ws_deque_push(&self.work, thread->parcel);
}

void scheduler_signal(cvar_t *cvar) {
  ustack_t *thread = cvar_pop_thread(cvar);
  if (thread)
    _resume(thread);
}


void scheduler_signal_all(struct cvar *cvar) {
  for (ustack_t *thread = cvar_pop_all(cvar); thread; thread = thread->next)
    _resume(thread);
}


void scheduler_signal_error(struct cvar *cvar, hpx_status_t code) {
  for (ustack_t *thread = cvar_set_error(cvar, code); thread;
       thread = thread->next)
    _resume(thread);
}


static void _call_continuation(hpx_addr_t target, hpx_action_t action,
                               const void *args, size_t len,
                               hpx_status_t status) {
  const size_t payload = sizeof(locality_cont_args_t) + len;
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, payload);
  assert(p);
  hpx_parcel_set_target(p, target);
  hpx_parcel_set_action(p, locality_call_continuation);

  // perform the single serialization
  locality_cont_args_t *cargs = hpx_parcel_get_data(p);
  cargs->action = action;
  cargs->status = status;
  memcpy(&cargs->data, args, len);
  hpx_parcel_send(p, HPX_NULL);
}


/// unified continuation handler
static void HPX_NORETURN _continue(hpx_status_t status, size_t size,
                                   const void *value,
                                   void (*cleanup)(void*), void *env) {
  // if there's a continuation future, then we set it, which could spawn a
  // message if the future isn't local
  hpx_parcel_t *parcel = self.current;
  hpx_action_t c_act = hpx_parcel_get_cont_action(parcel);
  hpx_addr_t c_target = hpx_parcel_get_cont_target(parcel);
  if ((c_target != HPX_NULL) && !hpx_action_eq(c_act, HPX_ACTION_NULL)) {
    // Double the credit so that we can pass it on to the continuation
    // without splitting it up.
    if (parcel->pid != HPX_NULL)
      --parcel->credit;
    if (hpx_action_eq(c_act, hpx_lco_set_action))
      hpx_call_with_continuation(c_target, c_act, value, size, HPX_NULL,
                                 HPX_ACTION_NULL);
    else
      _call_continuation(c_target, c_act, value, size, status);
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
  if (likely(status == HPX_SUCCESS) || unlikely(status == HPX_LCO_ERROR) ||
      unlikely(status == HPX_ERROR)) {
    hpx_parcel_t *parcel = self.current;
    if ((parcel->pid != HPX_NULL) && parcel->credit)
      parcel_recover_credit(parcel);
    _continue(status, 0, NULL, NULL, NULL);
    unreachable();
  }

  // If we're supposed to be resending, we want to send back an invalidation
  // our estimated owner for the parcel's target address, and then resend the
  // parcel.
  if (status == HPX_RESEND) {
    hpx_parcel_t *parcel = self.current;

    // Get a parcel to transfer to, and transfer using the resend continuation.
    hpx_parcel_t *to = _schedule(false, NULL);
    assert(to);
    assert(parcel_get_stack(to));
    assert(parcel_get_stack(to)->sp);
    thread_transfer(to, _resend_parcel, parcel);
    unreachable();
  }

  dbg_error("worker: unexpected status %d.\n", status);
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


hpx_pid_t hpx_thread_current_pid(void) {
  if (self.current == NULL)
    return HPX_NULL;
  return self.current->pid;
}


uint32_t hpx_thread_current_credit(void) {
  if (self.current == NULL)
    return 0;
  return parcel_get_credit(self.current);
}


int hpx_thread_get_tls_id(void) {
  ustack_t *stack = parcel_get_stack(self.current);
  if (stack->tls_id < 0)
    stack->tls_id = sync_fadd(&here->sched->next_tls_id, 1, SYNC_ACQ_REL);

  return stack->tls_id;
}


/// A thread_transfer() continuation that runs when a thread changes its
/// affinity. This puts the current thread into the mailbox specified in env.
///
/// @param     to The thread to transfer to.
/// @param     sp The stack pointer that we're transferring away from.
/// @param    env The environment passed in from the transferring thread.
///
/// @returns HPX_SUCCESS
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
  assert(self.current);
  assert(parcel_get_stack(self.current));

  // make sure affinity is in bounds
  affinity = affinity % here->sched->n_workers;
  parcel_get_stack(self.current)->affinity = affinity;

  if (affinity == self.id)
    return;

  hpx_parcel_t *to = _schedule(NULL, false);
  thread_transfer(to, _move_to, (void*)(intptr_t)affinity);
}
