// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <hpx/builtins.h>

#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/instrumentation.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>                      // used as thread-control block
#include <libhpx/process.h>
#include <libhpx/scheduler.h>
#include <libhpx/system.h>
#include <libhpx/worker.h>
#include "cvar.h"
#include "thread.h"
#include "termination.h"

#ifdef ENABLE_DEBUG
# define _transfer _debug_transfer
#else
# define _transfer thread_transfer
#endif

#ifdef ENABLE_INSTRUMENTATION
static inline void TRACE_WQSIZE(struct worker *w) {
  static const int class = INST_SCHED;
  static const int id = HPX_INST_EVENT_SCHED_WQSIZE;
  size_t size = sync_chase_lev_ws_deque_size(&w->work);
  inst_trace(class, id, size);
}

static inline void TRACE_PUSH_LIFO(hpx_parcel_t *p) {
  static const int class = INST_SCHED;
  static const int id = HPX_INST_EVENT_SCHED_PUSH_LIFO;
  inst_trace(class, id, p);
}

static inline void TRACE_POP_LIFO(hpx_parcel_t *p) {
  static const int class = INST_SCHED;
  static const int id = HPX_INST_EVENT_SCHED_POP_LIFO;
  inst_trace(class, id, p);
}

static inline void TRACE_STEAL_LIFO(hpx_parcel_t *p,
                                    const struct worker *victim) {
  static const int class = INST_SCHED;
  static const int id = HPX_INST_EVENT_SCHED_STEAL_LIFO;
  inst_trace(class, id, p, victim->id);
}
#else
# define TRACE_WQSIZE(w)
# define TRACE_PUSH_LIFO(p)
# define TRACE_POP_LIFO(p)
# define TRACE_STEAL_LIFO(p, v)
#endif

__thread struct worker *self = NULL;

/// This transfer wrapper is used for logging, debugging, and instrumentation.
///
/// Internally, it will perform it's pre-transfer operations, call
/// thread_transfer(), and then perform post-transfer operations on the return.
static int HPX_USED
_debug_transfer(hpx_parcel_t *p, thread_transfer_cont_t cont, void *env) {
  return thread_transfer(p, cont, env);
}

/// The pthread entry function for dedicated worker threads.
///
/// This is used by worker_create().
static void *_run(void *worker) {
  dbg_assert(here);
  dbg_assert(here->gas);
  dbg_assert(worker);

  worker_bind_self(worker);

  // Ensure that all of the threads have joined the address spaces.
  as_join(AS_REGISTERED);
  as_join(AS_GLOBAL);
  as_join(AS_CYCLIC);

  if (worker_start()) {
    dbg_error("failed to start processing lightweight threads.\n");
    return NULL;
  }

  // leave the global address space
  as_leave();

  return NULL;
}

/// Continue a parcel by invoking its parcel continuation.
///
/// @param            p The parent parcel (usually self->current).
/// @param       status ?
/// @param        nargs The number of arguments to continue.
/// @param         args The arguments we are continuing.
///
/// @returns HPX_SUCCESS or an error if parcel_launch fails.
static int
_continue_parcel(hpx_parcel_t *p, hpx_status_t status, int nargs, va_list *args)
{
  if (p->c_target == HPX_NULL || p->c_action == HPX_ACTION_NULL) {
    return HPX_SUCCESS;
  }

  // create the parcel to continue and transfer whatever credit we have
  hpx_parcel_t *c = parcel_create_va(p->c_target, p->c_action, HPX_NULL,
                                     HPX_ACTION_NULL, nargs, args);
  dbg_assert(c);
  c->credit = p->credit;
  p->credit = 0;
  return parcel_launch(c);
}

/// Get the nop parcel.
static hpx_parcel_t *
_get_nop_parcel(void) {
  hpx_addr_t target = HPX_HERE;
  hpx_pid_t pid = hpx_thread_current_pid();
  // use parcel_new instead of hpx_parcel_acquire so that we can filter out
  // scheduler_nop actions in instrumentation
  hpx_parcel_t *p = parcel_new(target, scheduler_nop, 0, 0, pid, NULL, 0);
  return p;
}

/// The entry function for all interrupts.
///
/// This is the function that will run when we transfer to an interrupt. It
/// uses action_execute() to execute the interrupt on the current stack, sends
/// the parcel continuation if necessary, and then returns. Interrupts are
/// required not to call hpx_thread_continue(), so all execution will return
/// here.
///
/// @param            p The parcel that describes the interrupt.
static void
_execute_interrupt(hpx_parcel_t *p) {
  INST_EVENT_PARCEL_RUN(p);
  int e = action_execute(p);
  INST_EVENT_PARCEL_END(p);

  switch (e) {
   case HPX_SUCCESS:
    log_sched("completed interrupt\n");
    _continue_parcel(p, HPX_SUCCESS, 0, NULL);
    return;
   case HPX_RESEND:
    log_sched("resending interrupt to %"PRIu64"\n", p->target);
    if (HPX_SUCCESS != parcel_launch(p)) {
      dbg_error("failed to resend parcel\n");
    }
    return;
   default:
    dbg_error("interrupt produced unexpected error %s.\n", hpx_strerror(e));
  }
  unreachable();
}

/// The entry function for all of the lightweight threads.
///
/// @param       parcel The parcel that describes the thread to run.
static void _execute_thread(hpx_parcel_t *p) {
  INST_EVENT_PARCEL_RUN(p);
  int e = action_execute(p);
  switch (e) {
    default:
     dbg_error("thread produced unhandled error %s.\n", hpx_strerror(e));
     return;
    case HPX_ERROR:
     dbg_error("thread produced error.\n");
     return;
    case HPX_RESEND:
     log_sched("resending interrupt to %"PRIu64"\n", p->target);
     if (HPX_SUCCESS != parcel_launch(p)) {
       dbg_error("failed to resend parcel\n");
     }
     return;
    case HPX_SUCCESS:
    case HPX_LCO_ERROR:
      hpx_thread_exit(e);
  }
  unreachable();
}

/// A thread_transfer() continuation that runs after a worker first starts its
/// scheduling loop, but before any user defined lightweight threads run.
static int _on_startup(hpx_parcel_t *to, void *sp, void *env) {
  // checkpoint my native stack pointer
  self->sp = sp;
  self->current = to;

#ifndef ENABLE_DEBUG
  // Register the native stack for use as the "task" stack.
  //
  // Tasks are unusual because we run them from the _run_task continuation,
  // which always runs "below" sp on the stack. This gets released in
  // _worker_shutdown.
  //
  // If we're in a debug build, don't register to avoid an issue
  // registering the VG stack.

  void *base = (char*)sp - here->config->stacksize;
  network_register_dma(here->network, base, here->config->stacksize, NULL);
#endif

  return HPX_SUCCESS;
}

/// Freelist a stack.
static void
_put_stack(struct worker *w, ustack_t *stack) {
  stack->next = w->stacks;
  w->stacks = stack;
  int32_t count = ++w->nstacks;
  int32_t limit = here->config->sched_stackcachelimit;
  if (limit < 0 || count <= limit) {
    return;
  }

  int32_t half = ceil_div_32(limit, 2);
  log_sched("flushing half of the stack freelist (%d)\n", half);
  while (count > half) {
    stack = w->stacks;
    w->stacks = stack->next;
    count = --w->nstacks;
    thread_delete(stack);
  }
}

/// Try and get a stack from the freelist for the parcel.
static ustack_t*
_try_get_stack(struct worker *w, hpx_parcel_t *p) {
  ustack_t *stack = w->stacks;
  if (stack) {
    w->stacks = stack->next;
    --w->nstacks;
    thread_init(stack, p, _execute_thread, stack->size);
  }
  return stack;
}

/// Create a new lightweight thread based on the parcel.
///
/// The newly created thread is runnable, and can be thread_transfer()ed to in
/// the same way as any other lightweight thread can be.
///
/// @param          p The parcel that is generating this thread.
///
/// @returns          The parcel @p, but with a valid stack.
static hpx_parcel_t *_try_bind(hpx_parcel_t *p) {
  dbg_assert(p);
  if (parcel_get_stack(p))
    return p;

  ustack_t *stack = _try_get_stack(self, p);
  if (!stack) {
    stack = thread_new(p, _execute_thread);
  }
  ustack_t *old = parcel_set_stack(p, stack);
  dbg_assert_str(!old, "replaced stack %p with %p in %p\n", (void*)old,
                 (void*)stack,
                 (void*)p);
  return p;
  (void)old;
}

/// Add a parcel to the top of the worker's work queue.
static void _spawn_lifo(struct worker *w, hpx_parcel_t *p) {
  dbg_assert(p->target != HPX_NULL);
  dbg_assert(action_table_get_handler(here->actions, p->action) != NULL);
  DEBUG_IF(action_is_task(here->actions, p->action)) {
    dbg_assert(!parcel_get_stack(p));
  }

  TRACE_PUSH_LIFO(p);
  uint64_t size = sync_chase_lev_ws_deque_push(&w->work, p);
  self->work_first = (size >= here->sched->wf_threshold);
}

/// Process the next available parcel from our work queue in a lifo order.
static hpx_parcel_t *_schedule_lifo(struct worker *w) {
  hpx_parcel_t *p = sync_chase_lev_ws_deque_pop(&w->work);
  TRACE_POP_LIFO(p);
  TRACE_WQSIZE(w);
  return p;
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
    TRACE_STEAL_LIFO(p, victim);
    profile_ctr(++w->stats.steals);
  }

  return p;
}

/// Send a mail message to another worker.
static void _send_mail(int id, hpx_parcel_t *p) {
  dbg_assert(id >= 0);
  struct worker *w = scheduler_get_worker(here->sched, id);
  sync_two_lock_queue_enqueue(&w->inbox, p);
}

/// Process my mail queue.
static void _handle_mail(struct worker *w) {
  hpx_parcel_t *parcels = NULL;
  hpx_parcel_t *p = NULL;
  while ((parcels = sync_two_lock_queue_dequeue(&w->inbox))) {
    while ((p = parcel_stack_pop(&parcels))) {
      profile_ctr(++self->stats.mail);
      _spawn_lifo(w, p);
    }
  }
}

/// A transfer continuation that frees the current parcel.
///
/// During normal thread termination, the current thread and parcel need to be
/// freed. This can only be done safely once we've transferred away from that
/// thread (otherwise we've freed a stack that we're currently running on). This
/// continuation performs that operation.
static int _free_parcel(hpx_parcel_t *to, void *sp, void *env) {
  hpx_parcel_t *prev = self->current;
  self->current = to;
  ustack_t *stack = parcel_get_stack(prev);
  parcel_set_stack(prev, NULL);
  if (stack) {
    _put_stack(self, stack);
  }
  hpx_parcel_release(prev);
  int status = (intptr_t)env;
  return status;
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
  if (stack) {
    _put_stack(self, stack);
  }
  hpx_parcel_send(prev, HPX_NULL);
  return HPX_SUCCESS;
}

/// Called by the worker from the scheduler loop to shut itself down.
///
/// This will transfer back to the original system stack, returning the shutdown
/// code. We release our registration of the pthread stack here as well.
static void _worker_shutdown(struct worker *w) {
#ifndef ENABLE_DEBUG
  void *base = (char*)w->sp - here->config->stacksize;
  network_release_dma(here->network, base, here->config->stacksize);
#endif

  void **sp = &w->sp;
  intptr_t shutdown = sync_load(&w->sched->shutdown, SYNC_ACQUIRE);
  INST_EVENT_PARCEL_END(self->current);
  _transfer((hpx_parcel_t*)&sp, _free_parcel, (void*)shutdown);
  unreachable();
}

static int _run_task(hpx_parcel_t *to, void *sp, void *env) {
  hpx_parcel_t *from = self->current;

  // If we're transferring from a task, then we want to delete the current
  // task's parcel. Otherwise we are transferring from a thread and we want to
  // checkpoint the current thread so that we can return to it and then push it
  // so we can find it later (or have it stolen later).
  if (action_is_task(here->actions, from->action)) {
    hpx_parcel_release(from);
  }
  else {
    parcel_get_stack(from)->sp = sp;
    _spawn_lifo(self, from);
  }

  self->current = env;
  dbg_assert(parcel_get_stack(self->current) == NULL);
  _execute_thread(env);
  unreachable();
  return HPX_SUCCESS;
}


/// Try to execute a parcel as a task.
///
/// @param            p The parcel to test.
///
/// @returns       NULL The parcel was processed as a task.
///                  @p The parcel was not a task.
static hpx_parcel_t *_try_task(hpx_parcel_t *p) {
  if (scheduler_is_shutdown(self->sched)) {
    return p;
  }

  if (!action_is_task(here->actions, p->action)) {
    return p;
  }

  dbg_assert(!parcel_get_stack(p));

  void **sp = &self->sp;

  // No instrumentation for suspend here since thread is already recorded as
  // suspended (we mark the suspend before the call to _schedule).
  int e = _transfer((hpx_parcel_t*)&sp, _run_task, p);
  dbg_check(e, "Error post _try_task: %s\n", hpx_strerror(e));
  return NULL;
}

/// Try to execute a parcel as an interrupt.
///
/// @param            p The parcel to test.
///
/// @returns       NULL The parcel was processed as a task.
///                  @p The parcel was not a task.
static hpx_parcel_t *_try_interrupt(hpx_parcel_t *p) {
  if (scheduler_is_shutdown(self->sched)) {
    return p;
  }

  if (!action_is_interrupt(here->actions, p->action)) {
    return p;
  }

  _execute_interrupt(p);
  hpx_parcel_release(p);
  return NULL;
}

/// Schedule something quickly.
///
/// This routine does not try very hard to find work, and is used inside of an
/// LCO when the caller is holding the LCO lock and wants to release it.
///
/// This routine will not process any tasks.
///
/// @param        final A thread to transfer to if we can't find any other
///                       work.
///
/// @returns            A parcel to transfer to.
static hpx_parcel_t *_schedule_in_lco(hpx_parcel_t *final) {
  hpx_parcel_t *p = NULL;

  // return so we can release the lock
  if (scheduler_is_shutdown(self->sched)) {
    p = _get_nop_parcel();
    goto exit;
  }

  // if there is any LIFO work, process it
  if ((p = _schedule_lifo(self))) {
    goto exit;
  }

  if ((p = final)) {
    goto exit;
  }

  p = _get_nop_parcel();
 exit:
  dbg_assert(p);
  return _try_bind(p);
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
/// @param    in_lco Schedule quickly so caller can release lco.
/// @param     final A final option if the scheduler wants to give up.
///
/// @returns A thread to transfer to.
static hpx_parcel_t *_schedule(bool in_lco, hpx_parcel_t *final) {
  if (in_lco) {
    return _schedule_in_lco(final);
  }

  hpx_parcel_t *p = NULL;

  // We spin in the scheduler processing tasks, until we find a parcel to run
  // that does not represent a task.
  while (p == NULL) {
    if (scheduler_is_shutdown(self->sched)) {
      _worker_shutdown(self);
    }

    // prioritize our mailbox
    _handle_mail(self);

    // if there is any LIFO work, process it
    p = _schedule_lifo(self);
    if (p) {
      p = _try_task(p);
      continue;
    }

    // we prioritize yielded threads over stealing
    p = _schedule_yielded(self);
    if (p) {
      p = _try_task(p);
      continue;
    }

    // try to steal some work
    p = _schedule_steal(self);
    if (p) {
      p = _try_task(p);
      continue;
    }

    // try to run the final, but only the first time around
    p = final;
    if (p) {
      p = _try_task(p);
      final = NULL;
      continue;
    }

    // couldn't find any work to do, we sleep for a while before looking again
    system_usleep(1);
  }

  return _try_bind(p);
}


int worker_init(struct worker *w, struct scheduler *sched, int id,
                unsigned seed, unsigned work_size)
{
  dbg_assert(w);
  dbg_assert(sched);

  /// make sure the worker has proper alignment
  dbg_assert(((uintptr_t)w & (HPX_CACHELINE_SIZE - 1)) == 0);
  dbg_assert(((uintptr_t)&w->work & (HPX_CACHELINE_SIZE - 1)) == 0);
  dbg_assert(((uintptr_t)&w->inbox & (HPX_CACHELINE_SIZE - 1)) == 0);

  w->sched      = sched;
  w->thread     = 0;
  w->id         = id;
  w->seed       = seed;
  w->work_first = 0;
  w->nstacks    = 0;
  w->sp         = NULL;
  w->current    = NULL;
  w->stacks     = NULL;

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

  // and delete any cached stacks
  ustack_t *stack = NULL;
  while ((stack = w->stacks)) {
    w->stacks = stack->next;
    thread_delete(stack);
  }
}

void worker_bind_self(struct worker *worker) {
  dbg_assert(worker);

  if (self && self != worker) {
    dbg_error("HPX does not permit worker structure switching.\n");
  }
  self = worker;
  self->thread = pthread_self();
}

int worker_start(void) {
  dbg_assert(self);

  // double-check this
  dbg_assert(((uintptr_t)self & (HPX_CACHELINE_SIZE - 1)) == 0);
  dbg_assert(((uintptr_t)&self->work & (HPX_CACHELINE_SIZE - 1)) == 0);
  dbg_assert(((uintptr_t)&self->inbox & (HPX_CACHELINE_SIZE - 1))== 0);

  // make sure the system is initialized
  dbg_assert(here && here->config && here->network);

  // wait for local threads to start up
  system_barrier_wait(&self->sched->barrier);

  // get a parcel to start the scheduler loop with
  hpx_parcel_t *p = _schedule(true, NULL);
  if (!p) {
    dbg_error("failed to acquire an initial parcel.\n");
  }

  int e = _transfer(p, _on_startup, NULL);
  if (e) {
    if (here->rank == 0) {
      log_error("application exited with a non-zero exit code: %d.\n", e);
    }
    return e;
  }

  self->current = NULL;

  return LIBHPX_OK;
}

int worker_create(struct worker *worker, const config_t *cfg) {
  pthread_t thread;

  int e = pthread_create(&thread, NULL, _run, worker);
  if (e) {
    dbg_error("failed to start a scheduler worker pthread.\n");
    return e;
  }
  return LIBHPX_OK;
}

void worker_join(struct worker *worker) {
  dbg_assert(worker);

  if (worker->thread == pthread_self()) {
    return;
  }

  int e = pthread_join(worker->thread, NULL);
  if (e) {
    dbg_error("cannot join worker thread %d (%s).\n", worker->id, strerror(e));
  }
}

void worker_cancel(struct worker *worker) {
  dbg_assert(worker);
  dbg_assert(worker->thread != pthread_self());
  if (pthread_cancel(worker->thread)) {
    dbg_error("cannot cancel worker thread %d.\n", worker->id);
  }
}

static int _work_first(hpx_parcel_t *to, void *sp, void *env) {
  hpx_parcel_t *prev = self->current;
  parcel_get_stack(prev)->sp = sp;
  self->current = to;
  _spawn_lifo(self, prev);
  return HPX_SUCCESS;
}

/// Spawn a parcel.
///
/// This complicated function does a bunch of logic to figure out the proper
/// method of computation for the parcel.
void scheduler_spawn(hpx_parcel_t *p) {
  dbg_assert(self);
  dbg_assert(self->id >= 0);
  dbg_assert(p);
  dbg_assert(hpx_gas_try_pin(p->target, NULL)); // just performs translation
  dbg_assert(action_table_get_handler(here->actions, p->action) != NULL);
  profile_ctr(self->stats.spawns++);

  // Don't run anything until we have started up. This lets us use parcel_send()
  // before hpx_run() without worrying about weird work-first or interrupt
  // effects.
  if (!self->sp) {
    _spawn_lifo(self, p);
    return;
  }

  // We always try and run interrupts eagerly. This should always be safe.
  p = _try_interrupt(p);
  if (!p) {
    return;
  }

  // If we're in help-first mode, or we're supposed to shutdown, we go ahead and
  // buffer this parcel. It will get pulled from the buffer later for
  // processing.
  if (!self->work_first || scheduler_is_shutdown(self->sched)) {
    _spawn_lifo(self, p);
    return;
  }

  // Otherwise we're in work-first mode, which means we should do our best to go
  // ahead and run the parcel ourselves. There are some things that inhibit
  // work-first scheduling.

  // 1) We can't work-first anything if we're running a task, because blocking
  //    the task will also block its parent.
  //
  //    NB: We probably can actually run tasks from tasks, since they're
  //        guaranteed not to block (note we already do this for interrupts),
  //        the issue is that we can't use _try_task() to do this because we get
  //        parcel releases that we don't want when we do that. We could
  //        restructure _try_task() to deal with that, in which case it would
  //        make sense to hoist the _try_task() farther up this decision tree.
  hpx_parcel_t *current = self->current;
  if (action_is_task(here->actions, current->action)) {
    _spawn_lifo(self, p);
    return;
  }

  // 2) We can't work-first if we are holding an LCO lock.
  ustack_t *thread = parcel_get_stack(current);
  if (thread->lco_depth) {
    _spawn_lifo(self, p);
    return;
  }

  // 3) We can't work-first from an interrupt.
  if (action_is_interrupt(here->actions, current->action)) {
    _spawn_lifo(self, p);
    return;
  }

  // We can process the parcel work-first, but we need to use a thread to do it
  // so that our continuation can be stolen.
  INST_EVENT_PARCEL_SUSPEND(current);
  int e = _transfer(_try_bind(p), _work_first, NULL);
  INST_EVENT_PARCEL_RESUME(current);
  dbg_check(e, "Detected a work-first scheduling error: %s\n", hpx_strerror(e));
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
static int
_checkpoint_yield(hpx_parcel_t *to, void *sp, void *env) {
  self->current = to;
  hpx_parcel_t *prev = env;
  parcel_get_stack(prev)->sp = sp;
  sync_two_lock_queue_enqueue(&here->sched->yielded, prev);
  return HPX_SUCCESS;
}

void
scheduler_yield(void) {
  hpx_parcel_t *from = self->current;
  if (!action_is_default(here->actions, from->action)) {
    // task or interrupt can't yield
    return;
  }

  // if there's nothing else to do, we can be rescheduled
  hpx_parcel_t *to = _schedule(false, from);
  if (from == to) {
    return;
  }

  dbg_assert(to);
  dbg_assert(parcel_get_stack(to));
  dbg_assert(parcel_get_stack(to)->sp);

  // note that we don't instrument yields because they overwhelm tracing
  _transfer(to, _checkpoint_yield, from);
}

void
hpx_thread_yield(void) {
  profile_ctr(++self->stats.yields);
  scheduler_yield();
}

/// A transfer continuation that unlocks a lock.
static int
_unlock(hpx_parcel_t *to, void *sp, void *env) {
  lockable_ptr_t *lock = env;
  hpx_parcel_t *prev = self->current;
  self->current = to;
  parcel_get_stack(prev)->sp = sp;
  sync_lockable_ptr_unlock(lock);
  return HPX_SUCCESS;
}

hpx_status_t
scheduler_wait(lockable_ptr_t *lock, cvar_t *condition) {
  // push the current thread onto the condition variable---no lost-update
  // problem here because we're holing the @p lock
  ustack_t *thread = parcel_get_stack(self->current);
  dbg_assert(thread->lco_depth == 1);
  hpx_status_t status = cvar_push_thread(condition, thread);
  if (status != HPX_SUCCESS) {
    return status;
  }

  INST_EVENT_PARCEL_SUSPEND(self->current);
  hpx_parcel_t *to = _schedule(true, NULL);
  _transfer(to, _unlock, (void*)lock);
  INST_EVENT_PARCEL_RESUME(self->current);

  // reacquire the lco lock before returning
  sync_lockable_ptr_lock(lock);
  return cvar_get_error(condition);
}

/// Resume list of parcels.
///
/// This scans through the passed list of parcels, and tests to see if it
/// corresponds to a thread (i.e., has a stack) or not. If its a thread, we
/// check to see if it has soft affinity and ship it to its home through a
/// mailbox of necessary. If its just a parcel, we use the launch infrastructure
/// to send it off.
///
/// @param      parcels A stack of parcels to resume.
static void
_resume(hpx_parcel_t *parcels) {
  hpx_parcel_t *p;
  while ((p = parcel_stack_pop(&parcels))){
    ustack_t *stack = parcel_get_stack(p);
    if (stack && stack->affinity >= 0) {
      _send_mail(stack->affinity, p);
    }
    else {
      parcel_launch(p);
    }
  }
}

void
scheduler_signal(cvar_t *cvar) {
  _resume(cvar_pop(cvar));
}

void
scheduler_signal_all(struct cvar *cvar) {
  _resume(cvar_pop_all(cvar));
}

void
scheduler_signal_error(struct cvar *cvar, hpx_status_t code) {
  _resume(cvar_set_error(cvar, code));
}

static void HPX_NORETURN
_continue(hpx_status_t status, void (*cleanup)(void*), void *env, int nargs,
          va_list *args)
{
  hpx_parcel_t *parcel = self->current;

  // send the parcel continuation---this takes my credit if I have any
  _continue_parcel(parcel, status, nargs, args);

  // run the cleanup handler
  if (cleanup != NULL) {
    cleanup(env);
  }

  // unpin the current target
  if (action_is_pinned(here->actions, parcel->action)) {
    hpx_gas_unpin(parcel->target);
  }

  // return any remaining credit
  process_recover_credit(parcel);

  hpx_parcel_t *to = _schedule(false, NULL);
  dbg_assert(to);
  dbg_assert(parcel_get_stack(to));
  dbg_assert(parcel_get_stack(to)->sp);
  INST_EVENT_PARCEL_END(parcel);
  _transfer(to, _free_parcel, (void*)(intptr_t)status);
  unreachable();
}

void
_hpx_thread_continue(int nargs, ...) {
  va_list vargs;
  va_start(vargs, nargs);
  _continue(HPX_SUCCESS, NULL, NULL, nargs, &vargs);
  va_end(vargs);
}

void
_hpx_thread_continue_cleanup(void (*cleanup)(void*), void *env, int nargs, ...)
{
  va_list vargs;
  va_start(vargs, nargs);
  _continue(HPX_SUCCESS, cleanup, env, nargs, &vargs);
  va_end(vargs);
}

void
hpx_thread_exit(int status) {
  hpx_parcel_t *parcel = self->current;

  if (status == HPX_RESEND) {
    // Get a parcel to transfer to, and transfer using the resend continuation.
    // NB: "fast" argument to schedule is "true" so that we don't try to run a
    //      task inside of schedule() which would cause the current task to be
    //      freed.
    hpx_parcel_t *to = _schedule(true, NULL);
    INST_EVENT_PARCEL_END(parcel);
    INST_EVENT_PARCEL_RESEND(parcel);
    _transfer(to, _resend_parcel, parcel);
    unreachable();
  }

  if (status == HPX_SUCCESS || status == HPX_LCO_ERROR || status == HPX_ERROR) {
    _continue(status, NULL, NULL, 0, NULL);
    unreachable();
  }

  dbg_error("unexpected exit status %d.\n", status);
  hpx_abort();
}

scheduler_stats_t *
thread_get_stats(void) {
  return (self) ? &self->stats : NULL;
}

hpx_parcel_t *
scheduler_current_parcel(void) {
  return self->current;
}

int
hpx_get_my_thread_id(void) {
  return (self) ? self->id : -1;
}

hpx_addr_t
hpx_thread_current_target(void) {
  return (self && self->current) ? self->current->target : HPX_NULL;
}

hpx_addr_t
hpx_thread_current_cont_target(void) {
  return (self && self->current) ? self->current->c_target : HPX_NULL;
}

hpx_action_t
hpx_thread_current_action(void) {
  return (self && self->current) ? self->current->action : HPX_ACTION_NULL;
}

hpx_action_t
hpx_thread_current_cont_action(void) {
  return (self && self->current) ? self->current->c_action : HPX_ACTION_NULL;
}

hpx_pid_t
hpx_thread_current_pid(void) {
  return (self && self->current) ? self->current->pid : HPX_NULL;
}

uint32_t
hpx_thread_current_credit(void) {
  return (self && self->current) ? self->current->credit : 0;
}

int
hpx_thread_get_tls_id(void) {
  ustack_t *stack = parcel_get_stack(self->current);
  if (stack->tls_id < 0) {
    stack->tls_id = sync_fadd(&here->sched->next_tls_id, 1, SYNC_ACQ_REL);
  }

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

void
hpx_thread_set_affinity(int affinity) {
  dbg_assert(affinity >= -1);
  dbg_assert(self->current);
  dbg_assert(parcel_get_stack(self->current));

  // make sure affinity is in bounds
  affinity = affinity % here->sched->n_workers;
  parcel_get_stack(self->current)->affinity = affinity;

  if (affinity == self->id) {
    return;
  }

  INST_EVENT_PARCEL_SUSPEND(self->current);
  hpx_parcel_t *to = _schedule(false, NULL);
  _transfer(to, _move_to, (void*)(intptr_t)affinity);
  INST_EVENT_PARCEL_RESUME(self->current);
}

/// The environment for the _checkpoint_launch_through continuation.
typedef struct {
  int (*f)(void *);
  void *env;
} _checkpoint_suspend_env_t;

/// This continuation updates the `self->current` pointer to record that we are
/// now running @p to, checkpoints the previous stack pointer in the previous
/// stack, and then runs the continuation described in @p env.
///
/// This continuation *does not* record the previous parcel in any scheduler
/// structures, it is completely invisible to the runtime. The expectation is
/// that the continuation in @p env will ultimately cause the parcel to resume.
///
/// @param           to The parcel we transferred to.
/// @param           sp The stack pointer we transferred from.
/// @param          env A _checkpoint_suspend_env_t that describes the closure.
///
/// @return             The status from the closure continuation.
static int
_checkpoint_suspend(hpx_parcel_t *to, void *sp, void *env) {
  hpx_parcel_t *prev = self->current;
  self->current = to;
  parcel_get_stack(prev)->sp = sp;
  _checkpoint_suspend_env_t *c = env;
  return c->f(c->env);
}

hpx_status_t
scheduler_suspend(int (*f)(void*), void *env) {
  // create the closure environment for the _checkpoint_suspend continuation
  _checkpoint_suspend_env_t suspend_env = {
    .f = f,
    .env = env
  };

  INST_EVENT_PARCEL_SUSPEND(self->current);
  // Don't block during the schedule call, we still have something to do (call
  // the continuation) that may impact global progress.
  hpx_parcel_t *to = _schedule(true, NULL);
  int e = _transfer(to, _checkpoint_suspend, &suspend_env);
  INST_EVENT_PARCEL_RESUME(self->current);
  return e;
}

intptr_t
worker_can_alloca(size_t bytes) {
  ustack_t *current = parcel_get_stack(self->current);
  return ((uintptr_t)&current - (uintptr_t)current->stack < bytes);
}
