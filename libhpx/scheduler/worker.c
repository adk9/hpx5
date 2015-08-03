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

#ifdef HAVE_APEX
#include "apex.h"
#include "apex_policies.h"
#endif

#ifdef ENABLE_DEBUG
# define _transfer _debug_transfer
#else
# define _transfer thread_transfer
#endif

#ifdef ENABLE_INSTRUMENTATION
static inline void TRACE_WQSIZE(worker_t *w) {
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
                                    const worker_t *victim) {
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

__thread worker_t * volatile self = NULL;

/// This transfer wrapper is used for logging, debugging, and instrumentation.
///
/// Internally, it will perform it's pre-transfer operations, call
/// thread_transfer(), and then perform post-transfer operations on the return.
static int HPX_USED
_debug_transfer(hpx_parcel_t *p, thread_transfer_cont_t cont, void *env) {
  return thread_transfer(p, cont, env);
}

#ifdef HAVE_APEX
void APEX_STOP(void) {
  if (self->profiler != NULL) {
    apex_stop((apex_profiler_handle)(self->profiler));
    self->profiler = NULL;
  }
  return;
}

void APEX_START(hpx_parcel_t *p) {
  // if this is NOT a null or lightweight action,
  // send a "start" event to APEX
  hpx_action_t id = hpx_parcel_get_action(p);
  if (id != HPX_ACTION_NULL && id != scheduler_nop &&
      id != hpx_lco_set_action) {
    void* handler = (void*)hpx_action_get_handler(id);
    self->profiler = (void*)(apex_start(APEX_FUNCTION_ADDRESS, handler));
  }
  return;
}

void APEX_RESUME(hpx_parcel_t *p) {
  if (p != NULL) {
    hpx_action_t id = hpx_parcel_get_action(p);
    // "resume" will not increment the number of calls.
    if (id != HPX_ACTION_NULL && id != scheduler_nop &&
        id != hpx_lco_set_action) {
      void* handler = (void*)hpx_action_get_handler(id);
      self->profiler = (void*)(apex_resume(APEX_FUNCTION_ADDRESS, handler));
    }
  }
  return;
}

#else
#define APEX_START(_p)
#define APEX_STOP()
#define APEX_RESUME(_p)
#endif

/// The pthread entry function for dedicated worker threads.
///
/// This is used by worker_create().
static void *
_run(void *worker) {
  dbg_assert(here);
  dbg_assert(here->gas);
  dbg_assert(worker);

  worker_bind_self(worker);

  // Ensure that all of the threads have joined the address spaces.
  as_join(AS_REGISTERED);
  as_join(AS_GLOBAL);
  as_join(AS_CYCLIC);

#ifdef HAVE_APEX
  // let APEX know there is a new thread
  apex_register_thread("HPX WORKER THREAD");
#endif

  if (worker_start()) {
    dbg_error("failed to start processing lightweight threads.\n");
    return NULL;
  }

  // leave the global address space
  as_leave();

  // unbind self
  self = NULL;

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
  hpx_parcel_t *c = action_create_parcel_va(p->c_target, p->c_action, HPX_NULL,
                                            HPX_ACTION_NULL, nargs, args);
  dbg_assert(c);
  c->credit = p->credit;
  p->credit = 0;
  return parcel_launch(c);
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
static void _execute_interrupt(hpx_parcel_t *p) {
  INST_EVENT_PARCEL_RUN(p);
  int e = action_execute(p);
  INST_EVENT_PARCEL_END(p);

  switch (e) {
   case HPX_SUCCESS:
    log_sched("completed interrupt\n");
    _continue_parcel(p, HPX_SUCCESS, 0, NULL);

    if (action_is_pinned(here->actions, p->action)) {
      hpx_gas_unpin(p->target);
    }

    process_recover_credit(p);
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
  APEX_START(p);
  int e = action_execute(p);
  APEX_STOP();
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

/// Swap the current parcel for a worker.
static hpx_parcel_t *_swap_current(hpx_parcel_t *p, void *sp, worker_t *w) {
  hpx_parcel_t *q = w->current;
  w->current = p;
  q->ustack->sp = sp;
  return q;
}

/// A thread_transfer() continuation that the native pthread stack uses to
/// transfer to work.
///
/// This transfer is special because it understands that there is no parcel in
/// self->current.
static int _transfer_from_native_thread(hpx_parcel_t *to, void *sp, void *env) {
  // checkpoint my native parcel
  hpx_parcel_t *prev = _swap_current(to, sp, self);
  dbg_assert(prev == self->system);
  return HPX_SUCCESS;
  (void)prev;                                  // avoid unused variable warnings
}

/// Freelist a parcel's stack.
static void _put_stack(hpx_parcel_t *p, worker_t *w) {
  ustack_t *stack = parcel_swap_stack(p, NULL);
  dbg_assert(stack);
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
static ustack_t *_try_get_stack(worker_t *w, hpx_parcel_t *p) {
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
static hpx_parcel_t *_try_bind(worker_t *w, hpx_parcel_t *p) {
  dbg_assert(p);
  if (p->ustack) {
    return p;
  }

  ustack_t *stack = _try_get_stack(w, p);
  if (!stack) {
    stack = thread_new(p, _execute_thread);
  }

  ustack_t *old = parcel_swap_stack(p, stack);
  DEBUG_IF(old != NULL) {
    dbg_error("Replaced stack %p with %p in %p: this usually means two workers "
              "are trying to start a lightweight thread at the same time.\n",
              (void*)old, (void*)stack, (void*)p);
  }
  return p;
  (void)old;                                    // avoid unused variable warning
}

/// Add a parcel to the top of the worker's work queue.
///
/// This interface is designed so that it can be used as a scheduler_suspend()
/// continuation.
///
/// @param            p The parcel that we're going to push.
/// @param       worker The worker pointer.
///
/// @return HPX_SUCCESS In order to match the scheduler_suspend() interface.
static int _push_lifo(hpx_parcel_t *p, void *worker) {
  dbg_assert(p->target != HPX_NULL);
  dbg_assert(action_table_get_handler(here->actions, p->action) != NULL);
  TRACE_PUSH_LIFO(p);
  worker_t *w = worker;
  uint64_t size = sync_chase_lev_ws_deque_push(&w->work, p);
  w->work_first = (here->sched->wf_threshold < size);
  return HPX_SUCCESS;
}

/// Process the next available parcel from our work queue in a lifo order.
static hpx_parcel_t *_schedule_lifo(worker_t *w) {
  hpx_parcel_t *p = sync_chase_lev_ws_deque_pop(&w->work);
  TRACE_POP_LIFO(p);
  TRACE_WQSIZE(w);
  return p;
}

/// Process the next available yielded thread.
static hpx_parcel_t *_schedule_yielded(worker_t *w) {
  return sync_two_lock_queue_dequeue(&here->sched->yielded);
}

/// Steal a lightweight thread during scheduling.
///
/// NB: we can be much smarter about who to steal from and how much to
/// steal. Ultimately though, we're building a distributed runtime so SMP work
/// stealing isn't that big a deal.
static hpx_parcel_t *_schedule_steal(worker_t *w) {
  int n = here->sched->n_workers;
  if (n == 1) {
    return NULL;
  }

  worker_t *victim = NULL;
  do {
    int id = rand_r(&w->seed) % n;
    victim = scheduler_get_worker(here->sched, id);
  } while (victim == w);

  hpx_parcel_t *p = sync_chase_lev_ws_deque_steal(&victim->work);
  if (p) {
    TRACE_STEAL_LIFO(p, victim);
    COUNTER_SAMPLE(++w->stats.steals);
  }

  return p;
}

/// Send a mail message to another worker.
///
/// This interface matches the scheduler_transfer continuation interface so that
/// it can be used in set_affinity.
static int _send_mail(hpx_parcel_t *p, void *worker) {
  worker_t *w = worker;
  log_sched("sending %p to worker %d\n", (void*)p, w->id);
  sync_two_lock_queue_enqueue(&w->inbox, p);
  return LIBHPX_OK;
}

/// Process my mail queue.
static void _handle_mail(worker_t *w) {
  hpx_parcel_t *parcels = NULL;
  hpx_parcel_t *p = NULL;
  while ((parcels = sync_two_lock_queue_dequeue(&w->inbox))) {
    while ((p = parcel_stack_pop(&parcels))) {
      COUNTER_SAMPLE(++w->stats.mail);
      _push_lifo(p, w);
    }
  }
}

/// A parcel_suspend continuation that frees the current parcel.
///
/// During normal thread termination, the current thread and parcel need to be
/// freed. This can only be done safely once we've transferred away from that
/// thread (otherwise we've freed a stack that we're currently running on). This
/// continuation performs that operation.
static int _free_parcel(hpx_parcel_t *p, void *env) {
  _put_stack(p, self);
  parcel_delete(p);
  return (intptr_t)env;
}

/// A parcel_suspend continuation that resends the current parcel.
///
/// If a parcel has arrived at the wrong locality because its target address has
/// been moved, then the application user will want to resend the parcel and
/// terminate the running thread. This transfer continuation performs that
/// operation.
///
/// The current thread is terminating however, so we release the stack we were
/// running on.
static int _resend_parcel(hpx_parcel_t *p, void *env) {
  _put_stack(p, self);
  hpx_parcel_send(p, HPX_NULL);
  return HPX_SUCCESS;
}

/// Try to execute a parcel as an interrupt.
///
/// @param            p The parcel to test.
///
/// @returns          1 The parcel was processed as an interrupt.
static int _try_interrupt(hpx_parcel_t *p) {
  if (scheduler_is_shutdown(here->sched)) {
    return 0;
  }

  if (!action_is_interrupt(here->actions, p->action)) {
    return 0;
  }

  _execute_interrupt(p);
  hpx_parcel_release(p);
  return 1;
}

/// Schedule something without blocking.
///
/// Typically this routine is used when the thread we are transferring away from
/// is going to do something in its continuation that includes
/// synchronization. If we block in _schedule() trying to find work to transfer
/// to, then we are blocking that continuation and could create performance
/// issues or deadlock.
///
/// @param        final A thread to transfer to if we can't find any other
///                       work.
///
/// @returns            A parcel to transfer to.
static hpx_parcel_t *
_schedule_nonblock(void) {
  // the scheduler has shutdown
  if (scheduler_is_shutdown(here->sched)) {
    return self->system;
  }

  hpx_parcel_t *p = _schedule_lifo(self);
  if (p) {
    return p;
  }

  return self->system;
}

/// This chunk of code is used in the scheduler for throttling concurrency.
#ifdef HAVE_APEX

/// This is the condition that "sleeping" threads will wait on
static pthread_cond_t release = PTHREAD_COND_INITIALIZER;

/// Mutex for the pthread_cond_wait() call. Typically, it is used
/// to save the shared state, but we don't need it for our use.
static pthread_mutex_t release_mutex = PTHREAD_MUTEX_INITIALIZER;

/// Flag to indicate whether this thread is processing a yield. If so,
/// it can't be throttled, because it could hold a lock that will be
/// requested by the thread trying to take the yielded work.
__thread bool _apex_yielded = false;

/// This function will check whether the current thread in the
/// scheduling loop should be throttled.
inline static int _apex_check_throttling(void) {
  // akp: throttling code -- if the throttling flag is set don't use
  // some threads NB: There is a possible race condition here. It's
  // possible that mail needs to be checked again just before
  // throttling.
    if (apex_get_throttle_concurrency()) {
      // am I inactive?
      if (self->active == false) {
        // because we can't change the power level, sleep instead.
        pthread_mutex_lock(&release_mutex);
        pthread_cond_wait(&release, &release_mutex);
        pthread_mutex_unlock(&release_mutex);
        // We've been signaled, see if we are the first to re-activate
        if (__sync_fetch_and_add(&(here->sched->n_active_workers),1) <=
            apex_get_thread_cap()) {
          // yes, we are good to go
          self->active = true;
          apex_set_state(APEX_BUSY);
        } else {
          // no, another thread beat us to it. This thread should stay idle.
          __sync_fetch_and_sub(&(here->sched->n_active_workers),1);
          return 1; // restart the scheduler loop
        }
      } else {
        if (apex_throttleOn && !_apex_yielded) {
          // randomly de-activate a thread?
          if (here->sched->n_active_workers > apex_get_thread_cap()) {
            // try to decrement the active worker counter
            if (__sync_fetch_and_sub(&(here->sched->n_active_workers),1) >
                apex_get_thread_cap()) {
              // success? then we are now inactive. go back to the top
              // of the loop.
              self->active = false;
              apex_set_state(APEX_THROTTLED);
              return 1; // restart the loop
            }
            // no, this thread should keep working.
            __sync_fetch_and_add(&(here->sched->n_active_workers),1);
          // has the thread cap increased?
          } else if (here->sched->n_active_workers < apex_get_thread_cap()) {
            // release a worker
            pthread_cond_signal(&release);
          }
        }
      }
    }
    return 0;
}

/// release idle threads, stop any running timers, and exit the thread from APEX
inline static void _apex_worker_shutdown(void) {
    pthread_cond_broadcast(&release);
    APEX_STOP();
    apex_exit_thread();
}
#endif


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
/// @param  nonblock Schedule without blocking.
/// @param     final A final option if the scheduler wants to give up.
///
/// @returns A thread to transfer to.
static hpx_parcel_t *
_schedule(bool nonblock, hpx_parcel_t *final) {
  hpx_parcel_t *p = NULL;
  hpx_parcel_t *(*yield_steal_0)(worker_t *);
  hpx_parcel_t *(*yield_steal_1)(worker_t *);
  int r;

  if (nonblock) {
    p = _schedule_nonblock();
  }

  // We spin in the scheduler until we can find some work to do.
  while (p == NULL) {
    if (scheduler_is_shutdown(here->sched)) {
      return self->system;
    }

    // prioritize our mailbox
    _handle_mail(self);

#ifdef HAVE_APEX
    if (_apex_check_throttling() != 0) {
      break;
    }
#endif

    // if there is any LIFO work, process it
    if ((p = _schedule_lifo(self))) {
      break;
    }

    // randomly determine if we yield or steal first
    r = rand_r(&self->seed);
    if (r < RAND_MAX/2) {
      // we prioritize yielded threads over stealing
      yield_steal_0 = _schedule_yielded;
      yield_steal_1 = _schedule_steal;
    }
    else {
      // try to steal some work first
      yield_steal_0 = _schedule_steal;
      yield_steal_1 = _schedule_yielded;
    }

    if ((p = yield_steal_0(self))) {
      break;
    }

    if ((p = yield_steal_1(self))) {
      break;
    }

    // try to run the final
    if ((p = final)) {
      break;
    }

    // couldn't find any work to do, we sleep for a while before looking again
    system_usleep(1);
  }

  return _try_bind(self, p);
}


int
worker_init(worker_t *w, int id, unsigned seed, unsigned work_size) {
  w->thread     = 0;
  w->id         = id;
  w->seed       = seed;
  w->work_first = 0;
  w->nstacks    = 0;
  w->in_lco     = 0;
  w->UNUSED     = 0;
  w->system     = NULL;
  w->current    = NULL;
  w->stacks     = NULL;
  w->active     = true;
  w->profiler   = NULL;

  sync_chase_lev_ws_deque_init(&w->work, work_size);
  sync_two_lock_queue_init(&w->inbox, NULL);
  libhpx_stats_init(&w->stats);

  return LIBHPX_OK;
}


void
worker_fini(worker_t *w) {
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

void
worker_bind_self(worker_t *worker) {
  dbg_assert(worker);

  if (self && self != worker) {
    dbg_error("HPX does not permit worker structure switching.\n");
  }
  self = worker;
  self->thread = pthread_self();
}

int
worker_start(void) {
  dbg_assert(self);

  // double-check this
  dbg_assert(((uintptr_t)self & (HPX_CACHELINE_SIZE - 1)) == 0);
  dbg_assert(((uintptr_t)&self->work & (HPX_CACHELINE_SIZE - 1)) == 0);
  dbg_assert(((uintptr_t)&self->inbox & (HPX_CACHELINE_SIZE - 1))== 0);

  // make sure the system is initialized
  dbg_assert(here && here->config && here->network);

  // wait for local threads to start up
  struct scheduler *sched = here->sched;
  system_barrier_wait(&sched->barrier);

  // allocate a parcel and a stack header for the system stack
  hpx_parcel_t p;
  parcel_init(0, 0, 0, 0, 0, NULL, 0, &p);
  ustack_t stack = {
    .sp = NULL,
    .parcel = &p,
    .next = NULL,
    .tls_id = -1,
    .stack_id = -1,
    .size = 0,
    .affinity = -1
  };
  p.ustack = &stack;

  self->system = &p;
  self->current = self->system;

  // the system thread will loop to find work until the scheduler has shutdown
  while (!scheduler_is_shutdown(sched)) {
    hpx_parcel_t *p = _schedule(0, NULL);
    if (self->system == p) {
      continue;
    }
    if (_transfer(p, _transfer_from_native_thread, NULL)) {
      break;
    }
  }

#ifdef HAVE_APEX
  _apex_worker_shutdown();
#endif

  if (sched->shutdown != HPX_SUCCESS) {
    if (here->rank == 0) {
      log_error("application exited with a non-zero exit code: %d.\n",
                sched->shutdown);
    }
    return sched->shutdown;
  }

  return LIBHPX_OK;
}

int
worker_create(worker_t *worker, const config_t *cfg) {
  pthread_t thread;

  int e = pthread_create(&thread, NULL, _run, worker);
  if (e) {
    dbg_error("failed to start a scheduler worker pthread.\n");
    return e;
  }
  return LIBHPX_OK;
}

void
worker_join(worker_t *worker) {
  dbg_assert(worker);

  if (worker->thread == pthread_self()) {
    return;
  }

  int e = pthread_join(worker->thread, NULL);
  if (e) {
    dbg_error("cannot join worker thread %d (%s).\n", worker->id, strerror(e));
  }
}

void
worker_cancel(worker_t *worker) {
  dbg_assert(worker);
  dbg_assert(worker->thread != pthread_self());
  if (pthread_cancel(worker->thread)) {
    dbg_error("cannot cancel worker thread %d.\n", worker->id);
  }
}


static int _work_first(hpx_parcel_t *to, void *sp, void *env) {
  worker_t *w = self;
  hpx_parcel_t *prev = w->current;
  w->current = to;
  prev->ustack->sp = sp;
  return _push_lifo(prev, w);
}

/// Spawn a parcel.
///
/// This complicated function does a bunch of logic to figure out the proper
/// method of computation for the parcel.
void
scheduler_spawn(hpx_parcel_t *p) {
  worker_t *w = self;
  dbg_assert(w);
  dbg_assert(w->id >= 0);
  dbg_assert(p);
  dbg_assert(hpx_gas_try_pin(p->target, NULL)); // just performs translation
  dbg_assert(action_table_get_handler(here->actions, p->action) != NULL);
  COUNTER_SAMPLE(w->stats.spawns++);

  // Don't run anything until we have started up.
  hpx_parcel_t *current = w->current;
  if (!current) {
    _push_lifo(p, w);
    return;
  }

  // If we're holding a lock then we have to push the spawn for later
  // processing, or we could end up causing a deadlock.
  if (w->in_lco) {
    _push_lifo(p, w);
    return;
  }

  // We always try and run interrupts eagerly.
  if (_try_interrupt(p)) {
    return;
  }

  // If we're not in work-first mode, then push the spawn for later.
  if (!w->work_first) {
    _push_lifo(p, w);
    return;
  }

  // If we are running an interrupt, then we can't work-first since we don't
  // have our own stack to suspend.
  if (action_is_interrupt(here->actions, current->action)) {
    _push_lifo(p, w);
    return;
  }

  // If we're shutting down then push the parcel and return. This prevents an
  // infinite spawn from inhibiting termination.
  if (scheduler_is_shutdown(here->sched)) {
    _push_lifo(p, w);
    return;
  }

  // Transfer to the parcel, checkpointing the current thread.
  INST_EVENT_PARCEL_SUSPEND(current);
  APEX_STOP();
  p = _try_bind(w, p);
  int e = _transfer(p, _work_first, NULL);
  dbg_check(e, "Detected a work-first scheduling error: %s\n", hpx_strerror(e));
  APEX_RESUME(current);
  INST_EVENT_PARCEL_RESUME(current);
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
  hpx_parcel_t *prev = _swap_current(to, sp, self);
  sync_two_lock_queue_enqueue(&here->sched->yielded, prev);
#ifdef HAVE_APEX
  _apex_yielded = false;
#endif
  return HPX_SUCCESS;
}

void scheduler_yield(void) {
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
  dbg_assert(to->ustack);
  dbg_assert(to->ustack->sp);

  // note that we don't instrument yields because they overwhelm tracing
  _transfer(to, _checkpoint_yield, NULL);
}

void
hpx_thread_yield(void) {
  COUNTER_SAMPLE(++self->stats.yields);
#ifdef HAVE_APEX
  _apex_yielded = true;
#endif
  scheduler_yield();
}

/// A transfer continuation that unlocks a lock.
///
/// We don't need to do anything with the previous parcel at this point because
/// we know that it has already been enqueued on whatever LCO we need it to be.
///
/// @param           to The parcel we are transferring to.
/// @param           sp The stack pointer we are transferring from.
/// @param         lock A lockable_ptr_t to unlock.
static int _unlock(hpx_parcel_t *to, void *sp, void *lock) {
  _swap_current(to, sp, self);
  dbg_assert(self->in_lco == 1);
  self->in_lco = 0;
  sync_lockable_ptr_unlock(lock);
  return HPX_SUCCESS;
}

hpx_status_t scheduler_wait(lockable_ptr_t *lock, cvar_t *condition) {
  // push the current thread onto the condition variable---no lost-update
  // problem here because we're holing the @p lock
  hpx_parcel_t *p = self->current;
  ustack_t *thread = p->ustack;

  // we had better be holding a lock here
  dbg_assert(self->in_lco > 0);

  hpx_status_t status = cvar_push_thread(condition, thread);
  if (status != HPX_SUCCESS) {
    return status;
  }

  INST_EVENT_PARCEL_SUSPEND(p);
  APEX_STOP();
  hpx_parcel_t *to = _schedule(true, NULL);
  _transfer(to, _unlock, (void*)lock);
  APEX_RESUME(p);
  INST_EVENT_PARCEL_RESUME(p);

  // reacquire the lco lock before returning
  sync_lockable_ptr_lock(lock);
  dbg_assert(self->in_lco == 0);
  self->in_lco = 1;
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
    ustack_t *stack = p->ustack;
    if (stack && stack->affinity >= 0) {
      worker_t *w = scheduler_get_worker(here->sched, stack->affinity);
      _send_mail(p, w);
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
_continue(worker_t *worker, hpx_status_t status, void (*cleanup)(void*),
          void *env, int nargs, va_list *args) {
  hpx_parcel_t *parcel = worker->current;

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

  INST_EVENT_PARCEL_END(parcel);
  APEX_STOP();
  scheduler_suspend(_free_parcel, (void*)(intptr_t)status, 1);
  unreachable();
}

void
_hpx_thread_continue(int nargs, ...) {
  va_list vargs;
  va_start(vargs, nargs);
  _continue(self, HPX_SUCCESS, NULL, NULL, nargs, &vargs);
  va_end(vargs);
}

void
_hpx_thread_continue_cleanup(void (*cleanup)(void*), void *env, int nargs, ...)
{
  va_list vargs;
  va_start(vargs, nargs);
  _continue(self, HPX_SUCCESS, cleanup, env, nargs, &vargs);
  va_end(vargs);
}

void hpx_thread_exit(int status) {
  worker_t *worker = self;
  hpx_parcel_t *parcel = worker->current;

  if (status == HPX_RESEND) {
    INST_EVENT_PARCEL_END(parcel);
    APEX_STOP();
    INST_EVENT_PARCEL_RESEND(parcel);
    scheduler_suspend(_resend_parcel, NULL, 0);
    unreachable();
  }

  if (status == HPX_SUCCESS || status == HPX_LCO_ERROR || status == HPX_ERROR) {
    _continue(worker, status, NULL, NULL, 0, NULL);
    unreachable();
  }

  dbg_error("unexpected exit status %d.\n", status);
  hpx_abort();
}

hpx_parcel_t *
scheduler_current_parcel(void) {
  return self->current;
}

int
hpx_get_my_thread_id(void) {
  worker_t *w = self;
  return (w) ? w->id : -1;
}

hpx_addr_t
hpx_thread_current_target(void) {
  worker_t *w = self;
  return (w && w->current) ? w->current->target : HPX_NULL;
}

hpx_addr_t
hpx_thread_current_cont_target(void) {
  worker_t *w = self;
  return (w && w->current) ? w->current->c_target : HPX_NULL;
}

hpx_action_t
hpx_thread_current_action(void) {
  worker_t *w = self;
  return (w && w->current) ? w->current->action : HPX_ACTION_NULL;
}

hpx_action_t
hpx_thread_current_cont_action(void) {
  worker_t *w = self;
  return (w && w->current) ? w->current->c_action : HPX_ACTION_NULL;
}

hpx_pid_t
hpx_thread_current_pid(void) {
  worker_t *w = self;
  return (w && w->current) ? w->current->pid : HPX_NULL;
}

uint32_t
hpx_thread_current_credit(void) {
  worker_t *w = self;
  return (w && w->current) ? w->current->credit : 0;
}

int
hpx_thread_get_tls_id(void) {
  worker_t *w = self;
  ustack_t *stack = w->current->ustack;
  if (stack->tls_id < 0) {
    stack->tls_id = sync_fadd(&here->sched->next_tls_id, 1, SYNC_ACQ_REL);
  }

  return stack->tls_id;
}

void
hpx_thread_set_affinity(int affinity) {
  // make sure affinity is in bounds
  dbg_assert(affinity >= -1);
  dbg_assert(affinity < here->sched->n_workers);

  worker_t *worker = self;
  dbg_assert(worker->current);
  dbg_assert(worker->current->ustack);
  dbg_assert(worker->current != worker->system);

  // set the soft affinity
  hpx_parcel_t  *p = worker->current;
  ustack_t *thread = p->ustack;
  thread->affinity = affinity;

  // if this is clearing the affinity return
  if (affinity < 0) {
    return;
  }

  // if this is already running on the correct worker return
  if (affinity == worker->id) {
    return;
  }

  // move this thread to the proper worker through the mailbox
  INST_EVENT_PARCEL_SUSPEND(p);
  APEX_STOP();
  worker_t *w = scheduler_get_worker(here->sched, affinity);
  scheduler_suspend(_send_mail, w, 0);
  APEX_RESUME(p);
  INST_EVENT_PARCEL_RESUME(p);
}

/// The environment for the _checkpoint_launch_through continuation.
typedef struct {
  int (*f)(hpx_parcel_t *, void *);
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
static int _checkpoint_suspend(hpx_parcel_t *to, void *sp, void *env) {
  hpx_parcel_t *prev = _swap_current(to, sp, self);
  _checkpoint_suspend_env_t *c = env;
  return c->f(prev, c->env);
}

hpx_status_t
scheduler_suspend(int (*f)(hpx_parcel_t *p, void*), void *env, int block) {
  // create the closure environment for the _checkpoint_suspend continuation
  _checkpoint_suspend_env_t suspend_env = {
    .f = f,
    .env = env
  };

  hpx_parcel_t *p = self->current;
  INST_EVENT_PARCEL_SUSPEND(p);
  log_sched("suspending %p in %s\n", (void*)p,
            action_table_get_key(here->actions, p->action));
  hpx_parcel_t *to = _schedule(!block, NULL);
  int e = _transfer(to, _checkpoint_suspend, &suspend_env);
  log_sched("resuming %p\n in %s", (void*)p,
            action_table_get_key(here->actions, p->action));
  INST_EVENT_PARCEL_RESUME(p);
  return e;
}

intptr_t
worker_can_alloca(size_t bytes) {
  ustack_t *current = self->current->ustack;
  return ((uintptr_t)&current - (uintptr_t)current->stack < bytes);
}
