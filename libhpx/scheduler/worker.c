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
#include <libhpx/termination.h>
#include <libhpx/topology.h>
#include <libhpx/worker.h>
#include "cvar.h"
#include "lco.h"
#include "thread.h"


/// Storage for the thread-local worker pointer.
__thread worker_t * volatile self = NULL;

#define SOURCE_LIFO 0
#define SOURCE_YIELD 1
#define SOURCE_STEAL 2
#define SOURCE_FINAL 3

static void EVENT_WQSIZE(worker_t *w) {
  static const int class = INST_SCHED;
  static const int id = HPX_INST_EVENT_SCHED_WQSIZE;
  inst_trace(class, id, sync_chase_lev_ws_deque_size(&w->queues[w->work_id].work));
}

static void EVENT_PUSH_LIFO(hpx_parcel_t *p) {
  static const int class = INST_SCHED;
  static const int id = HPX_INST_EVENT_SCHED_PUSH_LIFO;
  inst_trace(class, id, p);
}

static void EVENT_POP_LIFO(hpx_parcel_t *p) {
  static const int class = INST_SCHED;
  static const int id = HPX_INST_EVENT_SCHED_POP_LIFO;
  inst_trace(class, id, p);
}

static void EVENT_STEAL_LIFO(hpx_parcel_t *p, const worker_t *victim) {
  static const int class = INST_SCHED;
  static const int id = HPX_INST_EVENT_SCHED_STEAL_LIFO;
  inst_trace(class, id, p, victim->id);
}

static void EVENT_THREAD_RUN(hpx_parcel_t *p, worker_t *w) {
#ifdef HAVE_APEX
  // if this is NOT a null or lightweight action, send a "start" event to APEX
  if (p->action != hpx_lco_set_action) {
    void* handler = (void*)hpx_action_get_handler(p->action);
    w->profiler = (void*)(apex_start(APEX_FUNCTION_ADDRESS, handler));
  }
#endif
  static const int type = HPX_INST_CLASS_PARCEL;
  static const int id = HPX_INST_EVENT_PARCEL_RUN;
  inst_trace(type, id, p->id, p->action, p->size);
}

static void EVENT_THREAD_END(hpx_parcel_t *p, worker_t *w) {
#ifdef HAVE_APEX
  if (w->profiler != NULL) {
    apex_stop((apex_profiler_handle)(w->profiler));
    w->profiler = NULL;
  }
#endif
  static const int type = HPX_INST_CLASS_PARCEL;
  static const int id = HPX_INST_EVENT_PARCEL_END;
  inst_trace(type, id, p->id, p->action);
}

static void EVENT_THREAD_SUSPEND(hpx_parcel_t *p, worker_t *w) {
#ifdef HAVE_APEX
  if (w->profiler != NULL) {
    apex_stop((apex_profiler_handle)(w->profiler));
    w->profiler = NULL;
  }
#endif
  static const int type = HPX_INST_CLASS_PARCEL;
  static const int id = HPX_INST_EVENT_PARCEL_SUSPEND;
  inst_trace(type, id, p->id, p->action);
}

static void EVENT_THREAD_RESUME(hpx_parcel_t *p, worker_t *w) {
#ifdef HAVE_APEX
  if (p->action != hpx_lco_set_action) {
    void* handler = (void*)hpx_action_get_handler(p->action);
    w->profiler = (void*)(apex_resume(APEX_FUNCTION_ADDRESS, handler));
  }
#endif
  static const int type = HPX_INST_CLASS_PARCEL;
  static const int id = HPX_INST_EVENT_PARCEL_RESUME;
  inst_trace(type, id, p->id, p->action);
}

#ifdef ENABLE_DEBUG
/// This transfer wrapper is used for logging, debugging, and instrumentation.
///
/// Internally, it will perform it's pre-transfer operations, call
/// thread_transfer(), and then perform post-transfer operations on the return.
static void _dbg_transfer(hpx_parcel_t *p, thread_transfer_cont_t c, void *e) {
  thread_transfer(p, c, e);
}

# define _transfer _dbg_transfer
#else
# define _transfer thread_transfer
#endif

/// Continue a parcel by invoking its parcel continuation.
///
/// @param            p The parent parcel (usually self->current).
/// @param        nargs The number of arguments to continue.
/// @param         args The arguments we are continuing.
static void _continue_parcel(hpx_parcel_t *p, int nargs, va_list *args) {
  if (p->c_target == HPX_NULL || p->c_action == HPX_ACTION_NULL) {
    process_recover_credit(p);
    return;
  }

  // create the parcel to continue and transfer whatever credit we have
  hpx_parcel_t *c = action_create_parcel_va(p->c_target, p->c_action, HPX_NULL,
                                            HPX_ACTION_NULL, nargs, args);
  dbg_assert(c);
  c->credit = p->credit;
  p->credit = 0;
  parcel_launch(c);
}

/// Swap the current parcel for a worker.
static hpx_parcel_t *_swap_current(hpx_parcel_t *p, void *sp, worker_t *w) {
  hpx_parcel_t *q = w->current;
  w->current = p;
  q->ustack->sp = sp;
  return q;
}

/// The entry function for all interrupts.
///
/// This is the function that will run when we transfer to an interrupt. It
/// uses action_execute() to execute the interrupt on the current stack, sends
/// the parcel continuation if necessary, and then returns. Interrupts are
/// required not to call hpx_thread_continue(), or hpx_thread_exit(), so all
/// execution will return here.
///
/// @param            p The parcel that describes the interrupt.
static void _execute_interrupt(hpx_parcel_t *p) {
  // Exchange the current thread pointer for the duration of the call. This
  // allows the application code to access thread data, e.g., the current
  // target.
  worker_t *w = self;
  hpx_parcel_t *q = _swap_current(p, NULL, w);

  // "Borrow" the current thread's stack, so that we can use its lco_depth field
  // if necessary.
  dbg_assert(!p->ustack);
  p->ustack = q->ustack;

  EVENT_THREAD_RUN(p, w);
  int e = action_execute(p);
  EVENT_THREAD_END(p, w);

  // Restore the current thread pointer.
  _swap_current(q, NULL, w);
  p->ustack = NULL;

  switch (e) {
   case HPX_SUCCESS:
    log_sched("completed interrupt %p\n", p);
    _continue_parcel(p, 0, NULL);
    if (action_is_pinned(&actions, p->action)) {
      hpx_gas_unpin(p->target);
    }
    parcel_delete(p);
    break;

   case HPX_RESEND:
    log_sched("resending interrupt to %"PRIu64"\n", p->target);
    EVENT_PARCEL_RESEND(p);
    parcel_launch(p);
    break;

   case HPX_LCO_ERROR:
    dbg_error("interrupt returned LCO error %s.\n", hpx_strerror(e));

   case HPX_ERROR:
   default:
    dbg_error("interrupt produced unexpected error %s.\n", hpx_strerror(e));
  }
}

/// The entry function for all of the lightweight threads.
///
/// @param       parcel The parcel that describes the thread to run.
static void _execute_thread(hpx_parcel_t *p) {
  // matching events are in hpx_thread_exit()
  EVENT_THREAD_RUN(p, self);
  int e = action_execute(p);
  hpx_thread_exit(e);
  unreachable();
}

/// Create a new lightweight thread based on the parcel.
///
/// The newly created thread is runnable, and can be thread_transfer()ed to in
/// the same way as any other lightweight thread can be. Calling _try_bind() on
/// a parcel that already has a stack (i.e., a thread) is permissible and has no
/// effect.
///
/// @param          w The current worker (we will use its stack freelist)
/// @param          p The parcel that is generating this thread.
///
/// @returns          The parcel @p, but with a valid stack.
static hpx_parcel_t *_try_bind(worker_t *w, hpx_parcel_t *p) {
  if (p->ustack) {
    return p;
  }

  // try and get a stack from the freelist, otherwise allocate a new one
  ustack_t *stack = w->stacks;
  if (stack) {
    w->stacks = stack->next;
    --w->nstacks;
    thread_init(stack, p, _execute_thread, stack->size);
  }
  else {
    stack = thread_new(p, _execute_thread);
  }

  ustack_t *old = parcel_swap_stack(p, stack);
  DEBUG_IF(old != NULL) {
    dbg_error("Replaced stack %p with %p in %p: this usually means two workers "
              "are trying to start a lightweight thread at the same time.\n",
              (void*)old, (void*)stack, (void*)p);
  }

  // avoid unused variable warning
  (void)old;

  return p;
}

static chase_lev_ws_deque_t *_work(worker_t *worker) {
  return &worker->queues[worker->work_id].work;
}

static chase_lev_ws_deque_t *_yielded(worker_t *worker) {
  return &worker->queues[1 - worker->work_id].work;
}

static void _swap_epoch(worker_t *worker) {
  worker->work_id = 1 - worker->work_id;
}

/// Add a parcel to the top of the worker's work queue.
///
/// This interface is designed so that it can be used as a schedule()
/// continuation.
///
/// @param            p The parcel that we're going to push.
/// @param       worker The worker pointer.
static void _push_lifo(hpx_parcel_t *p, void *worker) {
  dbg_assert(p->target != HPX_NULL);
  dbg_assert(action_table_get_handler(&actions, p->action) != NULL);
  EVENT_PUSH_LIFO(p);
  worker_t *w = worker;
  uint64_t size = sync_chase_lev_ws_deque_push(_work(w), p);
  if (w->work_first < 0) {
    return;
  }
  w->work_first = (here->sched->wf_threshold < size);
}

/// Process the next available parcel from our work queue in a lifo order.
static hpx_parcel_t *_schedule_lifo(worker_t *w) {
  hpx_parcel_t *p = sync_chase_lev_ws_deque_pop(_work(w));
  EVENT_POP_LIFO(p);
  EVENT_WQSIZE(w);
  return p;
}

/// Send a mail message to another worker.
static void _send_mail(hpx_parcel_t *p, void *worker) {
  worker_t *w = worker;
  log_sched("sending %p to worker %d\n", (void*)p, w->id);
  sync_two_lock_queue_enqueue(&w->inbox, p);
}

/// Process my mail queue.
static void _handle_mail(worker_t *w) {
  hpx_parcel_t *parcels = NULL;
  hpx_parcel_t *p = NULL;
  while ((parcels = sync_two_lock_queue_dequeue(&w->inbox))) {
    while ((p = parcel_stack_pop(&parcels))) {
      COUNTER_SAMPLE(++w->stats.mail);
      log_sched("got mail %p\n", p);
      _push_lifo(p, w);
    }
  }
}

/// The action that pushes half the work to a thief.
static HPX_ACTION_DECL(_push_half);
static int _push_half_handler(int src) {
  const int steal_half_threshold = 6;
  log_sched("received push half request from worker %d\n", src);
  worker_t *thief = scheduler_get_worker(here->sched, src);
  int qsize = sync_chase_lev_ws_deque_size(_work(self));
  if (qsize < steal_half_threshold) {
    return HPX_SUCCESS;
  }

  // pop half of the total parcels from my work queue
  int count = (qsize >> 1);
  hpx_parcel_t *parcels = NULL;
  for (int i = 0; i < count; ++i) {
    hpx_parcel_t *p = _schedule_lifo(self);
    if (!p) {
      continue;
    }

    // if we find multiple push-half requests, purge the other ones
    if (p->action == _push_half) {
      parcel_delete(p);
      count++;
      continue;
    }
    parcel_stack_push(&parcels, p);
  }

  // send them back to the thief
  if (parcels) {
    _send_mail(parcels, thief);
    EVENT_STEAL_LIFO(parcels, self);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, 0, _push_half, _push_half_handler, HPX_INT);

/// Steal a parcel from a worker with the given @p id.
static hpx_parcel_t *_steal_from(worker_t *w, int id) {
  worker_t *victim = scheduler_get_worker(here->sched, id);
  hpx_parcel_t *p = sync_chase_lev_ws_deque_steal(_work(victim));
  if (p) {
    w->last_victim = id;
    EVENT_STEAL_LIFO(p, victim);
  } else {
    w->last_victim = -1;
  }
  return p;
}

/// Steal a parcel from a random worker out of all workers.
static hpx_parcel_t *_steal_random_all(worker_t *w) {
  int id;
  do {
    id = rand_r(&w->seed) % here->sched->n_workers;
  } while (id == w->id);
  return _steal_from(w, id);
}

/// Steal a parcel from a random worker from the same NUMA node.
static hpx_parcel_t *_steal_random_node(worker_t *w) {
  int id;
  do {
    int cpu = rand_r(&w->seed) % here->topology->cpus_per_node;
    id = here->topology->numa_to_cpus[w->numa_node][cpu];
  } while (id == w->id);
  return _steal_from(w, id);
}

/// Hierarchical work-stealing policy.
///
/// This policy is only applicable if the worker threads are
/// pinned. This policy works as follows:
///
/// 1. try to steal from the last succesful victim in
///    the same numa domain.
/// 2. if failed, try to steal randomly from the same numa domain.
/// 3. if failed, repeat step 2.
/// 4. if failed, try to steal half randomly from across the numa domain.
/// 5. if failed, go idle.
///
static hpx_parcel_t *_steal_hier(worker_t *w) {

  // disable hierarchical stealing if the worker threads are not
  // bound, or if the system is not hierarchical.
  libhpx_thread_affinity_t policy = here->config->thread_affinity;
  if (unlikely(policy == HPX_THREAD_AFFINITY_NONE ||
               here->topology->numa_to_cpus == NULL)) {
    return _steal_random_all(w);
  }

  dbg_assert(w->numa_node >= 0);
  hpx_parcel_t *p;

  // step 1
  int cpu = w->last_victim;
  if (cpu >= 0) {
    p = _steal_from(w, cpu);
    if (p) {
      return p;
    }
  }

  // step 2
  p = _steal_random_node(w);
  if (p) {
    return p;
  } else {
    // step 3
    p = _steal_random_node(w);
    if (p) {
      return p;
    }
  }

  // step 4
  int numa_node;
  do {
    numa_node = rand_r(&w->seed) % here->topology->nnodes;
  } while (numa_node == w->numa_node);

  int idx = rand_r(&w->seed) % here->topology->cpus_per_node;
  cpu = here->topology->numa_to_cpus[numa_node][idx];

  worker_t *victim = scheduler_get_worker(here->sched, cpu);
  p = action_create_parcel(HPX_HERE, _push_half, HPX_NULL,
                           HPX_ACTION_NULL, 1, &w->id);
  _send_mail(p, victim);
  return NULL;
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

  hpx_parcel_t *p;
  libhpx_sched_policy_t policy = here->config->sched_policy;
  switch (policy) {
    default:
      log_dflt("invalid scheduling policy, defaulting to random..");
    case HPX_SCHED_POLICY_DEFAULT:
    case HPX_SCHED_POLICY_RANDOM:
      p = _steal_random_all(w);
      break;
    case HPX_SCHED_POLICY_HIER:
      p = _steal_hier(w);
      break;
  }

  if (p) {
    COUNTER_SAMPLE(++w->stats.steals);
  } else {
    COUNTER_SAMPLE(++w->stats.failed_steals);
  }
  return p;
}

/// Freelist a parcel's stack.
static void _free_stack(hpx_parcel_t *p, worker_t *w) {
  ustack_t *stack = parcel_swap_stack(p, NULL);
  if (!stack) {
    return;
  }

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

/// A _schedule() continuation that frees the current parcel.
static void _free_parcel(hpx_parcel_t *p, void *env) {
  _free_stack(p, self);
  parcel_delete(p);
}

/// A _schedule() continuation that resends the current parcel.
static void _resend_parcel(hpx_parcel_t *p, void *env) {
  _free_stack(p, self);
  parcel_launch(p);
}

/// The environment for the _checkpoint() _transfer() continuation.
typedef struct {
  void (*f)(hpx_parcel_t *, void *);
  void *env;
} _checkpoint_env_t;

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
/// @param          env A _checkpoint_env_t that describes the closure.
///
/// @return             The status from the closure continuation.
static void _checkpoint(hpx_parcel_t *to, void *sp, void *env) {
  hpx_parcel_t *prev = _swap_current(to, sp, self);
  _checkpoint_env_t *c = env;
  c->f(prev, c->env);
}

/// Probe and progress the network.
static void _schedule_network(worker_t *w, network_t *network) {
  // suppress work-first scheduling while we're inside the network.
  w->work_first = -1;
  network_progress(network, 0);
  hpx_parcel_t *stack = network_probe(network, 0);
  w->work_first = 0;

  hpx_parcel_t *p = NULL;
  while ((p = parcel_stack_pop(&stack))) {
    EVENT_PARCEL_RECV(p);
    _push_lifo(p, w);
  }
}

/// The main scheduling loop.
///
/// Selects a new lightweight thread to run and transfers to it. After the
/// transfer has occurred, i.e. the worker has switched stacks and replaced its
/// current parcel, but before returning to user code, the scheduler will
/// execute @p f, passing it the previous parcel and the @p env specified
/// there.
///
/// If the @p block parameter is 1 then the scheduler can block before running
/// @p f. This is common, e.g., when the thread calling _schedule() is actually
/// shutting down. At other times running @p f can be important for progress or
/// performance, so the scheduler won't block. Blocking occurs in lightly loaded
/// situations where the scheduler can't find work to do.
///
/// @param            f A transfer continuation.
/// @param          env The transfer continuation environment.
/// @param        block It's okay to block before running @p f.
///
/// @returns            The status from _transfer.
static void _schedule(void (*f)(hpx_parcel_t *, void*), void *env, int block) {
  INST(uint64_t start_time = hpx_time_from_start_ns(hpx_time_now()));
  int source = -1;
  int spins = 0;
  hpx_parcel_t *p = NULL;
  worker_t *w = self;
  while (!worker_is_stopped()) {
    if (!block) {
      p = _schedule_lifo(w);
      if (INSTRUMENTATION && p != NULL) {
        source = SOURCE_LIFO;
      }
      break;
    }

    _handle_mail(w);

    // If we're not supposed to be active, then don't schedule anything.
    if (!worker_is_active()) {
      // make sure we don't have anything stuck in our yield queue
      while ((p = sync_chase_lev_ws_deque_pop(_yielded(w)))) {
        _push_lifo(p, w);
      }
      continue;
    }

    // See if we have primary lifo work.
    if ((p = _schedule_lifo(w))) {
      INST(source = SOURCE_LIFO);
      break;
    }

    // Swap our yield queue with our primary queue
    _swap_epoch(w);

    // Do some network stuff;
    _schedule_network(w, here->network);

    // Try and steal some work
    if ((p = _schedule_steal(w))) {
      source = SOURCE_STEAL;
      log_sched("stole work %p\n", p);
      break;
    }

    // couldn't find any work to do, eagerly spin
    INST(spins++);
  }

  // This somewhat clunky expression just makes sure that, if we found a parcel
  // to transfer to then it has a stack, or if we didn't find anything to
  // transfer to then pick the system stack
  p = (p) ? _try_bind(w, p) : w->system;

  inst_trace(HPX_INST_SCHEDTIMES, HPX_INST_SCHEDTIMES_SCHED,
    start_time, source, spins);

  // don't transfer to the same parcel
  if (p != w->current) {
    _transfer(p, _checkpoint, &(_checkpoint_env_t){ .f = f, .env = env });
  }

  (void)source;
  (void)spins;
}

int worker_init(worker_t *w, int id, unsigned seed, unsigned work_size) {
  w->thread      = 0;
  w->id          = id;
  w->seed        = seed;
  w->work_first  = 0;
  w->nstacks     = 0;
  w->yielded     = 0;
  w->last_victim = -1;
  w->system      = NULL;
  w->current     = NULL;
  w->stacks      = NULL;
  w->work_id     = 0;
  w->active      = true;
  w->profiler    = NULL;

  sync_chase_lev_ws_deque_init(&w->queues[0].work, work_size);
  sync_chase_lev_ws_deque_init(&w->queues[1].work, work_size);
  sync_two_lock_queue_init(&w->inbox, NULL);
  libhpx_stats_init(&w->stats);

  return LIBHPX_OK;
}


void worker_fini(worker_t *w) {
  // clean up the mailbox
  _handle_mail(w);
  sync_two_lock_queue_fini(&w->inbox);

  // and clean up the workqueue parcels
  hpx_parcel_t *p = NULL;
  while ((p = _schedule_lifo(w))) {
    parcel_delete(p);
  }
  sync_chase_lev_ws_deque_fini(&w->queues[0].work);
  sync_chase_lev_ws_deque_fini(&w->queues[1].work);

  // and delete any cached stacks
  ustack_t *stack = NULL;
  while ((stack = w->stacks)) {
    w->stacks = stack->next;
    thread_delete(stack);
  }
}

static void _null(hpx_parcel_t *p, void *env) {
}

int worker_start(void) {
  worker_t *w = self;
  dbg_assert(w);

  // double-check this
  dbg_assert(((uintptr_t)w & (HPX_CACHELINE_SIZE - 1)) == 0);
  dbg_assert(((uintptr_t)&w->queues & (HPX_CACHELINE_SIZE - 1)) == 0);
  dbg_assert(((uintptr_t)&w->inbox & (HPX_CACHELINE_SIZE - 1))== 0);

  // make sure the system is initialized
  dbg_assert(here && here->config && here->network);

  // affinitize the worker thread
  libhpx_thread_affinity_t policy = here->config->thread_affinity;
  int status = system_set_worker_affinity(w->id, policy);
  if (status != LIBHPX_OK) {
    log_dflt("WARNING: running with no worker thread affinity. "
             "This MAY result in diminished performance.\n");
  }

  int cpu = w->id % here->topology->ncpus;
  w->numa_node = here->topology->cpu_to_numa[cpu];

  // allocate a parcel and a stack header for the system stack
  hpx_parcel_t p;
  parcel_init(0, 0, 0, 0, 0, NULL, 0, &p);
  ustack_t stack = {
    .sp = NULL,
    .parcel = &p,
    .next = NULL,
    .lco_depth = 0,
    .tls_id = -1,
    .stack_id = -1,
    .size = 0,
    .affinity = -1
  };
  p.ustack = &stack;

  w->system = &p;
  w->current = w->system;
  w->work_first = 0;

  struct scheduler *sched = here->sched;
  while (true) {
    int stop = worker_is_stopped();
    if (stop && w->id == 0) {
      break;
    }

    if (stop) {
      int state = 0;
      pthread_mutex_lock(&sched->run_state.lock);
      while ((state = sched->run_state.state) == SCHED_STOP) {
        pthread_cond_wait(&sched->run_state.running, &sched->run_state.lock);
      }
      pthread_mutex_unlock(&sched->run_state.lock);
      if (state == SCHED_SHUTDOWN) {
        break;
      }
    }

    _schedule(_null, NULL, 1);
  }

  w->system      = NULL;
  w->current     = NULL;

  int code = sched->stopped;
  if (code != HPX_SUCCESS && here->rank == 0) {
    log_error("application exited with a non-zero exit code: %d.\n", code);
  }

  return code;
}

/// Spawn a parcel.
///
/// This complicated function does a bunch of logic to figure out the proper
/// method of computation for the parcel.
void scheduler_spawn(hpx_parcel_t *p) {
  worker_t *w = self;
  dbg_assert(w);
  dbg_assert(w->id >= 0);
  dbg_assert(p);
  dbg_assert(hpx_gas_try_pin(p->target, NULL)); // just performs translation
  dbg_assert(action_table_get_handler(&actions, p->action) != NULL);
  COUNTER_SAMPLE(w->stats.spawns++);

  // If we're stopped down then push the parcel and return. This prevents an
  // infinite spawn from inhibiting termination.
  if (worker_is_stopped()) {
    _push_lifo(p, w);
    return;
  }

  // If we're not in work-first mode, then push the parcel for later.
  if (w->work_first < 1) {
    _push_lifo(p, w);
    return;
  }

  hpx_parcel_t *current = w->current;

  // If we're holding a lock then we have to push the spawn for later
  // processing, or we could end up causing a deadlock.
  if (current->ustack->lco_depth) {
    _push_lifo(p, w);
    return;
  }

  // At this point, if we are spawning an interrupt, just run it.
  if (action_is_interrupt(&actions, p->action)) {
    _execute_interrupt(p);
    return;
  }

  // If we are running an interrupt, then we can't work-first since we don't
  // have our own stack to suspend.
  if (action_is_interrupt(&actions, current->action)) {
    _push_lifo(p, w);
    return;
  }

  // Process p work-first. If we're running the system thread then we need to
  // prevent it from being stolen, which we can do by using the NULL
  // continuation.
  _checkpoint_env_t env = {
    .f = (current == w->system) ? _null : _push_lifo,
     .env = w
  };

  EVENT_THREAD_SUSPEND(current, w);
  _transfer(_try_bind(w, p), _checkpoint, &env);
  EVENT_THREAD_RESUME(current, self);
}

/// This is the _schedule() continuation that we use to yield a thread.
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
static void _yield(hpx_parcel_t *p, void *env) {
  // sync_two_lock_queue_enqueue(&here->sched->yielded, p);
  worker_t *w = self;
  sync_chase_lev_ws_deque_push(_yielded(w), p);
  w->yielded = 0;
}

void scheduler_yield(void) {
  if (action_is_default(&actions, self->current->action)) {
    COUNTER_SAMPLE(++self->stats.yields);
    self->yielded = true;
    // NB: no trace point, overwhelms infrastructure
    _schedule(_yield, NULL, 0);
  }
}

/// A _schedule() continuation that unlocks a lock.
///
/// We don't need to do anything with the previous parcel at this point because
/// we know that it has already been enqueued on whatever LCO we need it to be.
///
/// @param           to The parcel we are transferring to.
/// @param         lock A lockable_ptr_t to unlock.
static void _unlock(hpx_parcel_t *to, void *lock) {
  sync_lockable_ptr_unlock(lock);
}

hpx_status_t scheduler_wait(lockable_ptr_t *lock, cvar_t *condition) {
  // push the current thread onto the condition variable---no lost-update
  // problem here because we're holing the @p lock
  worker_t *w = self;
  hpx_parcel_t *p = w->current;
  ustack_t *thread = p->ustack;

  // we had better be holding a lock here
  dbg_assert(thread->lco_depth > 0);

  hpx_status_t status = cvar_push_thread(condition, thread);
  if (status != HPX_SUCCESS) {
    return status;
  }

  EVENT_THREAD_SUSPEND(p, w);
  _schedule(_unlock, (void*)lock, 0);
  EVENT_THREAD_RESUME(p, self);

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
static void _resume_parcels(hpx_parcel_t *parcels) {
  hpx_parcel_t *p;
  while ((p = parcel_stack_pop(&parcels))) {
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

void scheduler_signal(cvar_t *cvar) {
  _resume_parcels(cvar_pop(cvar));
}

void scheduler_signal_all(struct cvar *cvar) {
  _resume_parcels(cvar_pop_all(cvar));
}

void scheduler_signal_error(struct cvar *cvar, hpx_status_t code) {
  _resume_parcels(cvar_set_error(cvar, code));
}

static void HPX_NORETURN
_continue(worker_t *worker, void (*cleanup)(void*), void *env, int nargs,
          va_list *args) {
  hpx_parcel_t *parcel = worker->current;

  // send the parcel continuation---this takes my credit if I have any
  _continue_parcel(parcel, nargs, args);

  // run the cleanup handler
  if (cleanup != NULL) {
    cleanup(env);
  }

  // unpin the current target
  if (action_is_pinned(&actions, parcel->action)) {
    hpx_gas_unpin(parcel->target);
  }

  EVENT_THREAD_END(parcel, worker);
  _schedule(_free_parcel, NULL, 1);
  unreachable();
}

void _hpx_thread_continue(int nargs, ...) {
  va_list vargs;
  va_start(vargs, nargs);
  _continue(self, NULL, NULL, nargs, &vargs);
  va_end(vargs);
}

void
_hpx_thread_continue_cleanup(void (*cleanup)(void*), void *env, int nargs, ...)
{
  va_list vargs;
  va_start(vargs, nargs);
  _continue(self, cleanup, env, nargs, &vargs);
  va_end(vargs);
}

void hpx_thread_exit(int status) {
  switch (status) {
   case HPX_RESEND:
    EVENT_THREAD_END(self->current, self);
    EVENT_PARCEL_RESEND(self->current);
    _schedule(_resend_parcel, NULL, 0);
    unreachable();
   case HPX_ERROR:
   case HPX_SUCCESS:
    _continue(self, NULL, NULL, 0, NULL);
    unreachable();
   case HPX_LCO_ERROR:
    // rewrite to lco_error
    self->current->c_action = lco_error;
    hpx_thread_continue(&status, sizeof(status));
    unreachable();
   default:
    dbg_error("unexpected exit status %d.\n", status);
  }
}

hpx_parcel_t *scheduler_current_parcel(void) {
  return self->current;
}

void hpx_thread_yield(void) {
  scheduler_yield();
}

int hpx_get_my_thread_id(void) {
  worker_t *w = self;
  return (w) ? w->id : -1;
}

hpx_addr_t hpx_thread_current_target(void) {
  worker_t *w = self;
  return (w && w->current) ? w->current->target : HPX_NULL;
}

hpx_addr_t hpx_thread_current_cont_target(void) {
  worker_t *w = self;
  return (w && w->current) ? w->current->c_target : HPX_NULL;
}

hpx_action_t hpx_thread_current_action(void) {
  worker_t *w = self;
  return (w && w->current) ? w->current->action : HPX_ACTION_NULL;
}

hpx_action_t hpx_thread_current_cont_action(void) {
  worker_t *w = self;
  return (w && w->current) ? w->current->c_action : HPX_ACTION_NULL;
}

hpx_pid_t hpx_thread_current_pid(void) {
  worker_t *w = self;
  return (w && w->current) ? w->current->pid : HPX_NULL;
}

uint32_t hpx_thread_current_credit(void) {
  worker_t *w = self;
  return (w && w->current) ? w->current->credit : 0;
}

int hpx_thread_get_tls_id(void) {
  ustack_t *stack = self->current->ustack;
  if (stack->tls_id < 0) {
    stack->tls_id = sync_fadd(&here->sched->next_tls_id, 1, SYNC_ACQ_REL);
  }

  return stack->tls_id;
}

intptr_t hpx_thread_can_alloca(size_t bytes) {
  ustack_t *current = self->current->ustack;
  return (uintptr_t)&current - (uintptr_t)current->stack - bytes;
}

void hpx_thread_set_affinity(int affinity) {
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
  EVENT_THREAD_SUSPEND(p, worker);
  worker_t *w = scheduler_get_worker(here->sched, affinity);
  _schedule(_send_mail, w, 0);
  EVENT_THREAD_RESUME(p, self);
}

void scheduler_suspend(void (*f)(hpx_parcel_t *, void*), void *env, int block) {
  worker_t *w = self;
  hpx_parcel_t *p = w->current;
  log_sched("suspending %p in %s\n", (void*)p,
            action_table_get_key(&actions, p->action));
  EVENT_THREAD_SUSPEND(p, w);
  _schedule(f, env, block);
  EVENT_THREAD_RESUME(p, self);
  log_sched("resuming %p\n in %s", (void*)p,
            action_table_get_key(&actions, p->action));
  (void)p;
}

