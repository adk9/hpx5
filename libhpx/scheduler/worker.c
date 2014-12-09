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
# include "config.h"
#endif

/// @file libhpx/scheduler/worker.c
/// @brief Implementation of the scheduler worker thread.

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <hpx/builtins.h>

#include <libsync/barriers.h>

#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>                      // used as thread-control block
#include <libhpx/process.h>
#include <libhpx/scheduler.h>
#include <libhpx/system.h>
#include <libhpx/worker.h>
#include "cvar.h"
#include "thread.h"
#include "termination.h"

static __thread struct worker *self = NULL;


/// The pthread entry function for dedicated worker threads.
///
/// This is used by worker_create().
static void *_run(void *worker) {
  assert(here);
  assert(here->gas);
  assert(worker);

  worker_bind_self(worker);

  if (gas_join(here->gas)) {
    dbg_error("failed to join the global address space.\n");
    return NULL;
  }

  if (worker_start()) {
    dbg_error("failed to start processing lightweight threads.\n");
    return NULL;
  }

  // leave the global address space
  gas_leave(here->gas);
  return NULL;
}


/// The entry function for all of the lightweight threads.
///
/// This entry function extracts the action and the arguments from the parcel,
/// and then invokes the action on the arguments. If the action returns to this
/// entry function, we dispatch to the correct thread termination handler.
///
/// @param       parcel The parcel that describes the thread to run.
static void HPX_NORETURN _thread_enter(hpx_parcel_t *parcel) {
  int status = action_invoke(parcel);
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
static int _on_startup(hpx_parcel_t *to, void *sp, void *env) {
  // checkpoint my native stack pointer
  self->sp = sp;
  self->current = to;

  // wait for the rest of the scheduler to catch up to me
  sync_barrier_join(here->sched->barrier, self->id);

  return HPX_SUCCESS;
}


/// Create a new lightweight thread based on the parcel.
///
/// The newly created thread is runnable, and can be thread_transfer()ed to in
/// the same way as any other lightweight thread can be.
///
/// @param          p The parcel that is generating this thread.

static void _try_bind(hpx_parcel_t *p) {
  assert(p);
  if (!parcel_get_stack(p)) {
    ustack_t *stack = thread_new(p, _thread_enter);
    parcel_set_stack(p, stack);
  }
}


/// Add a parcel to the top of the worker's work queue.
static void _spawn_lifo(struct worker *w, hpx_parcel_t *p) {
  sync_chase_lev_ws_deque_push(&w->work, p);
}



/// Process the next available parcel from our work queue in a lifo order.
static hpx_parcel_t *_schedule_lifo(struct worker *w) {
  return sync_chase_lev_ws_deque_pop(&w->work);
}


/// Process the next available yielded thread.
static hpx_parcel_t *_schedule_yielded(struct worker *w) {
  return sync_two_lock_queue_dequeue(&w->sched->yielded);
}


/// Steal a lightweight thread during scheduling.
///
/// NB: we can be much smarter about who to steal from and how much to
/// steal. Ultimately though, we're building a distributed runtime so SMP work
/// stealing isn't that big a deal.
static hpx_parcel_t *_schedule_steal(struct worker *w) {
  if (w->sched->n_workers == 1)
    return NULL;

  struct worker *victim = NULL;
  do {
    int id = rand_r(&w->seed) % w->sched->n_workers;
    victim = scheduler_get_worker(here->sched, id);
  } while (victim == w);

  hpx_parcel_t *p = sync_chase_lev_ws_deque_steal(&victim->work);
  if (p) {
    profile_ctr(++w->stats.steals);
  }

  return p;
}


/// Send a mail message to another worker.
static void _send_mail(int id, hpx_parcel_t *p) {
  assert(id >= 0);
  struct worker *w = scheduler_get_worker(here->sched, id);
  sync_two_lock_queue_enqueue(&w->inbox, p);
}


/// Process my mail queue.
static void _handle_mail(struct worker *w) {
  hpx_parcel_t *p = NULL;
  while ((p = sync_two_lock_queue_dequeue(&w->inbox))) {
    profile_ctr(++self->stats.mail);
    _spawn_lifo(w, p);
  }
}


/// A transfer continuation that frees the current parcel.
///
/// During normal thread termination, the current thread and parcel need to be
/// freed. This can only be done safely once we've transferred away from that
/// thread (otherwise we've freed a stack that we're currently running on). This
/// continuation performs that operation.
static int _free_parcel(hpx_parcel_t *to, void *sp, void *env) {
  self->current = to;
  hpx_parcel_t *prev = env;
  ustack_t *stack = parcel_get_stack(prev);
  parcel_set_stack(prev, NULL);
  thread_delete(stack);
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
  self->current = to;
  hpx_parcel_t *prev = env;
  ustack_t *stack = parcel_get_stack(prev);
  parcel_set_stack(prev, NULL);
  thread_delete(stack);
  hpx_parcel_send(prev, HPX_NULL);
  return HPX_SUCCESS;
}


static void _try_shutdown(struct worker *w) {
  if (!scheduler_running(w->sched)) {
    void **sp = &w->sp;
    thread_transfer((hpx_parcel_t*)&sp, _free_parcel, w->current);
    unreachable();
  }
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
  hpx_parcel_t *p = NULL;

  while (true) {
    _try_shutdown(self);

    // prioritize our mailbox
    _handle_mail(self);

    // if there is any LIFO work, process it
    p = _schedule_lifo(self);
    if (p) {
      break;
    }

    // if we're in a hurry just return final or a NULL action
    if (fast) {
      p = (final) ? final : hpx_parcel_acquire(NULL, 0);
      break;
    }

    // we prioritize yielded threads over stealing
    p = _schedule_yielded(self);
    if (p) {
      break;
    }

    // try to steal some work
    p = _schedule_steal(self);
    if (p) {
      break;
    }
  }

  assert(p);
  _try_bind(p);
  return p;
}


int worker_init(struct worker *w, struct scheduler *sched, int id, int core,
                 unsigned seed, unsigned work_size)
{
  assert(w);
  assert(sched);

  /// make sure the worker has proper alignment
  assert((uintptr_t)w % HPX_CACHELINE_SIZE == 0);
  assert((uintptr_t)&w->work % HPX_CACHELINE_SIZE == 0);
  assert((uintptr_t)&w->inbox % HPX_CACHELINE_SIZE == 0);

  w->sched      = sched;
  w->thread     = 0;
  w->id         = id;
  w->core       = core;
  w->seed       = seed;
  w->UNUSED     = 0;
  w->sp         = NULL;
  w->current    = NULL;

  sync_chase_lev_ws_deque_init(&w->work, work_size);
  sync_two_lock_queue_init(&w->inbox, NULL);
  scheduler_stats_init(&w->stats);

  return LIBHPX_OK;
}


void worker_fini(struct worker *w) {
  // clean up the mailbox
  _handle_mail(w);
  sync_two_lock_queue_fini(&w->inbox);

  // and clean up the workqueue parcels
  hpx_parcel_t *p = NULL;
  while ((p = _schedule_lifo(w))) {
    hpx_parcel_release(p);
  }
  sync_chase_lev_ws_deque_fini(&w->work);
}

void worker_bind_self(struct worker *worker) {
  assert(worker);

  if (self && self != worker) {
    dbg_error("HPX does not permit worker structure switching.\n");
  }
  self = worker;
  self->thread = pthread_self();

  // we set worker thread affinity in worker_create()
  //if (self->core >= 0) {
  //   dbg_log_sched("binding affinity for worker %d to core %d.\n", self->id, self->core);
  //  system_set_affinity(self->thread, self->core);
  //}
}

int worker_start(void) {
  assert(self);

  // get a parcel to start the scheduler loop with
  hpx_parcel_t *p = _schedule(true, NULL);
  if (!p) {
    dbg_error("failed to acquire an initial parcel.\n");
    return LIBHPX_ERROR;
  }

  if (thread_transfer(p, _on_startup, NULL)) {
    dbg_error("shutdown returned error.\n");
    return LIBHPX_ERROR;
  }

  return LIBHPX_OK;
}


int worker_create(struct worker *worker) {
  pthread_t thread;

  int e = pthread_create(&thread, NULL, _run, worker);
  if (e) {
    dbg_error("failed to start a scheduler worker pthread.\n");
    return e;
  }

  // unbind the worker thread (-1 indicates run on all available cores)
  system_set_affinity(thread, -1);

  return LIBHPX_OK;
}


void worker_join(struct worker *worker) {
  if (worker->thread != pthread_self()) {
    if (pthread_join(worker->thread, NULL)) {
      dbg_error("cannot join worker thread %d.\n", worker->id);
    }
  }
}


void worker_cancel(struct worker *worker) {
  assert(worker);
  if (pthread_cancel(worker->thread)) {
    dbg_error("cannot cancel worker thread %d.\n", worker->id);
  }
}


/// Spawn a user-level thread.
void scheduler_spawn(hpx_parcel_t *p) {
  assert(self);
  assert(self->id >= 0);
  assert(p);
  assert(hpx_gas_try_pin(hpx_parcel_get_target(p), NULL)); // NULL doesn't pin
  profile_ctr(self->stats.spawns++);
  _spawn_lifo(self, p);
}


/// Spawn a user-level task.
void scheduler_spawn_task(hpx_parcel_t *p) {
  int status = action_invoke(p);
  switch (status) {
    default:
      dbg_error("action: produced unhandled error %i.\n", (int)status);
      hpx_shutdown(status);
    case HPX_ERROR:
      dbg_error("action: produced error.\n");
      hpx_abort();
    case HPX_RESEND:
      hpx_parcel_send(p, HPX_NULL);
    case HPX_SUCCESS:
    case HPX_LCO_ERROR:
      process_recover_credit(p);
      hpx_action_t c_act = hpx_parcel_get_cont_action(p);
      hpx_addr_t c_target = hpx_parcel_get_cont_target(p);
      if ((c_target != HPX_NULL) && c_act != HPX_ACTION_NULL) {
        if (p->pid != HPX_NULL) {
          --p->credit;
        }

        if (c_act == hpx_lco_set_action) {
          hpx_call_with_continuation(c_target, c_act, NULL, 0, HPX_NULL,
                                     HPX_ACTION_NULL);
        } else {
          locality_cont_args_t cargs = { .action = c_act,
                                         .status = status };
          hpx_call_with_continuation(c_target, locality_call_continuation, &cargs,
                                     sizeof(cargs),
                                     HPX_NULL, HPX_ACTION_NULL);
        }
      }
      break;
  }
}


/// This is the continuation that we use to yield a thread.
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
static int _checkpoint_yield(hpx_parcel_t *to, void *sp, void *env) {
  self->current = to;
  hpx_parcel_t *prev = env;
  parcel_get_stack(prev)->sp = sp;
  sync_two_lock_queue_enqueue(&here->sched->yielded, prev);
  return HPX_SUCCESS;
}


void scheduler_yield(void) {
  // if there's nothing else to do, we can be rescheduled
  hpx_parcel_t *from = self->current;
  hpx_parcel_t *to = _schedule(false, from);
  if (from == to)
    return;

  assert(to);
  assert(parcel_get_stack(to));
  assert(parcel_get_stack(to)->sp);
  // transfer to the new thread
  thread_transfer(to, _checkpoint_yield, self->current);
}


void hpx_thread_yield(void) {
  scheduler_yield();
}


/// A transfer continuation that unlocks a lock.
static int _unlock(hpx_parcel_t *to, void *sp, void *env) {
  lockable_ptr_t *lock = env;
  hpx_parcel_t *prev = self->current;
  self->current = to;
  parcel_get_stack(prev)->sp = sp;
  sync_lockable_ptr_unlock(lock);
  return HPX_SUCCESS;
}


hpx_status_t scheduler_wait(lockable_ptr_t *lock, cvar_t *condition) {
  // push the current thread onto the condition variable---no lost-update
  // problem here because we're holing the @p lock
  ustack_t *thread = parcel_get_stack(self->current);
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
  while (thread) {
    if (thread->affinity >= 0 && thread->affinity != self->id) {
      _send_mail(thread->affinity, thread->parcel);
    }
    else {
      _spawn_lifo(self, thread->parcel);
    }
    thread = thread->next;
  }
}


void scheduler_signal(cvar_t *cvar) {
  _resume(cvar_pop_thread(cvar));
}


void scheduler_signal_all(struct cvar *cvar) {
  _resume(cvar_pop_all(cvar));
}


void scheduler_signal_error(struct cvar *cvar, hpx_status_t code) {
  _resume(cvar_set_error(cvar, code));
}


static void _call_continuation(hpx_addr_t target, hpx_action_t action,
                               const void *args, size_t len,
                               hpx_status_t status)
{
  // get a parcel we can use to call locality_call_continuation().
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(locality_cont_args_t) + len);
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
                                   void (*cleanup)(void*), void *env)
{
  hpx_parcel_t *parcel = self->current;
  hpx_action_t c_act = hpx_parcel_get_cont_action(parcel);
  hpx_addr_t c_target = hpx_parcel_get_cont_target(parcel);
  if ((c_target != HPX_NULL) && c_act != HPX_ACTION_NULL) {
    // Double the credit so that we can pass it on to the continuation
    // without splitting it up.
    if (parcel->pid != HPX_NULL) {
      --parcel->credit;
    }

    if (c_act == hpx_lco_set_action) {
      hpx_call_with_continuation(c_target, c_act, value, size, HPX_NULL,
                                 HPX_ACTION_NULL);
    }
    else {
      _call_continuation(c_target, c_act, value, size, status);
    }
  }

  // run the cleanup handler
  if (cleanup != NULL) {
    cleanup(env);
  }

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
  hpx_parcel_t *parcel = self->current;

  if (status == HPX_RESEND) {
    // Get a parcel to transfer to, and transfer using the resend continuation.
    hpx_parcel_t *to = _schedule(false, NULL);
    thread_transfer(to, _resend_parcel, parcel);
    unreachable();
  }

  if (status == HPX_SUCCESS || status == HPX_LCO_ERROR || status == HPX_ERROR) {
    process_recover_credit(parcel);
    _continue(status, 0, NULL, NULL, NULL);
    unreachable();
  }

  dbg_error("unexpected exit status %d.\n", status);
  hpx_abort();
}


scheduler_stats_t *thread_get_stats(void) {
  if (self) {
    return &self->stats;
  }
  else {
    return NULL;
  }
}


hpx_parcel_t *scheduler_current_parcel(void) {
  return self->current;
}


int hpx_get_my_thread_id(void) {
  return (self) ? self->id : -1;
}


hpx_addr_t hpx_thread_current_target(void) {
  return (self && self->current) ? self->current->target : HPX_NULL;
}


hpx_addr_t hpx_thread_current_cont_target(void) {
  return (self && self->current) ? self->current->c_target : HPX_NULL;
}


hpx_action_t hpx_thread_current_cont_action(void) {
  return (self && self->current) ? self->current->c_action : HPX_ACTION_NULL;
}


uint32_t hpx_thread_current_args_size(void) {
  return (self && self->current) ? self->current->size : 0;
}


hpx_pid_t hpx_thread_current_pid(void) {
  return (self && self->current) ? self->current->pid : HPX_NULL;
}


uint32_t hpx_thread_current_credit(void) {
  return (self && self->current) ? parcel_get_credit(self->current) : 0;
}


int hpx_thread_get_tls_id(void) {
  ustack_t *stack = parcel_get_stack(self->current);
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
  hpx_parcel_t *prev = self->current;
  self->current = to;
  parcel_get_stack(prev)->sp = sp;

  // just send the previous parcel to the targeted worker
  _send_mail((int)(intptr_t)env, prev);
  return HPX_SUCCESS;
}


void hpx_thread_set_affinity(int affinity) {
  assert(affinity >= -1);
  assert(self->current);
  assert(parcel_get_stack(self->current));

  // make sure affinity is in bounds
  affinity = affinity % here->sched->n_workers;
  parcel_get_stack(self->current)->affinity = affinity;

  if (affinity == self->id) {
    return;
  }

  hpx_parcel_t *to = _schedule(false, NULL);
  thread_transfer(to, _move_to, (void*)(intptr_t)affinity);
}
