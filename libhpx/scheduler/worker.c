// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/events.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/instrumentation.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>                      // used as thread-control block
#include <libhpx/process.h>
#include <libhpx/rebalancer.h>
#include <libhpx/scheduler.h>
#include <libhpx/system.h>
#include <libhpx/termination.h>
#include <libhpx/topology.h>
#include <libhpx/worker.h>
#include "cvar.h"
#include "events.h"
#include "thread.h"
#include "lco/lco.h"

/// Storage for the thread-local worker pointer.
__thread worker_t * volatile self = NULL;

/// Macro to record a parcel's GAS accesses.
#if defined(HAVE_AGAS) && defined(HAVE_REBALANCING)
# define GAS_TRACE_ACCESS(src, dst, block, size) \
  rebalancer_add_entry(src, dst, block, size)
#elif defined(ENABLE_INSTRUMENTATION)
# define GAS_TRACE_ACCESS EVENT_GAS_ACCESS
#else
# define GAS_TRACE_ACCESS(src, dst, block, size)
#endif

/// An enumeration used during instrumentation to define where we got work
enum {
  LIFO = 0,
  YIELD,
  STEAL,
  FINAL,
  SYSTEM
} TRANSFER_SOURCES;

/// Swap a worker's current parcel.
///
/// @param            p The parcel we're swapping in.
/// @param           sp The checkpointed stack pointer for the current parcel.
/// @param            w The current worker.
///
/// @@returns           The previous parcel.
static hpx_parcel_t *_swap_current(hpx_parcel_t *p, void *sp, worker_t *w) {
  hpx_parcel_t *q = w->current;
  w->current = p;
  q->ustack->sp = sp;
  return q;
}

/// The environment for the _checkpoint() _transfer() continuation.
typedef struct {
  void (*f)(hpx_parcel_t *, void *);
  void *env;
} _checkpoint_env_t;

/// The basic transfer continuation used by the scheduler.
///
/// This transfer continuation updates the `self->current` pointer to record
/// that we are now running @p to, checkpoints the previous stack pointer in the
/// previous stack, and then runs the continuation described in @p env.
///
/// @param           to The parcel we transferred to.
/// @param           sp The stack pointer we transferred from.
/// @param          env A _checkpoint_env_t that describes the closure.
static void _checkpoint(hpx_parcel_t *to, void *sp, void *env) {
  hpx_parcel_t *prev = _swap_current(to, sp, self);
  _checkpoint_env_t *c = env;
  c->f(prev, c->env);
}

/// Local wrapper for the thread transfer call.
///
/// This wrapper will reset the signal mask as part of the transfer if it is
/// necessary, and it will always checkpoint the stack for the thread that we
/// are transferring away from.
///
/// @param            p The parcel to transfer to.
/// @param            f The checkpoint continuation.
/// @param            e The checkpoint continuation environment.
/// @param            w The current worker.
static void _transfer(hpx_parcel_t *p, void (*f)(hpx_parcel_t *, void *),
                      void *e, worker_t *w) {
  if (!w->current->ustack->masked) {
    thread_transfer(p, _checkpoint, &(_checkpoint_env_t) { .f = f, .env = e });
  }
  else {
    sigset_t set;
    dbg_check(pthread_sigmask(SIG_SETMASK, &here->mask, &set));
    thread_transfer(p, _checkpoint, &(_checkpoint_env_t) { .f = f, .env = e });
    dbg_check(pthread_sigmask(SIG_SETMASK, &set, NULL));
  }
}

hpx_parcel_t *_hpx_thread_generate_continuation(int n, ...) {
  worker_t *w = self;
  hpx_parcel_t *p = w->current;

  dbg_assert(p->ustack->cont == 0);

  hpx_action_t op = p->c_action;
  hpx_addr_t target = p->c_target;
  va_list args;
  va_start(args, n);
  hpx_parcel_t *c = action_new_parcel_va(op, target, 0, 0, n, &args);
  va_end(args);

  p->ustack->cont = 1;
  p->c_action = 0;
  p->c_target = 0;
  return c;
}

/// Continue a parcel by invoking its parcel continuation.
///
/// @param            p The parent parcel (usually self->current).
/// @param            n The number of arguments to continue.
/// @param         args The arguments we are continuing.
static void _vcontinue_parcel(hpx_parcel_t *p, int n, va_list *args) {
  dbg_assert(p->ustack->cont == 0);
  p->ustack->cont = 1;
  if (p->c_action && p->c_target) {
    action_continue_va(p->c_action, p, n, args);
  }
  else {
    process_recover_credit(p);
  }
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
  dbg_assert(!p->ustack);

  // Exchange the current thread pointer for the duration of the call. This
  // allows the application code to access thread data, e.g., the current
  // target.
  worker_t *w = self;
  hpx_parcel_t *q = _swap_current(p, NULL, w);
  ustack_t *stack = q->ustack;

  // "Borrow" the current thread's stack, so that we can use its lco_depth and
  // cont fields if necessary.
  p->ustack = stack;

  short cont = stack->cont;
  short masked = stack->masked;
  stack->cont = 0;
  stack->masked = 0;

  sigset_t mask;
  if (masked) {
    dbg_check(pthread_sigmask(SIG_SETMASK, &here->mask, &mask));
  }

  // Suspend the outer thread, and start the interrupt
  EVENT_THREAD_SUSPEND(q, w);
  EVENT_THREAD_RUN(p, w);
  EVENT_SCHED_END(0, 0);
  int e = action_exec_parcel(p->action, p);
  EVENT_SCHED_BEGIN();

  switch (e) {
   case HPX_SUCCESS:
    log_sched("completed interrupt %p\n", p);
    if (!stack->cont) {
      _vcontinue_parcel(p, 0, NULL);
    }
    _swap_current(q, NULL, w);
    p->ustack = NULL;
    EVENT_THREAD_END(p, w);
    EVENT_THREAD_RESUME(q, self);
    parcel_delete(p);
    break;

   case HPX_RESEND:
    log_sched("resending interrupt to %"PRIu64"\n", p->target);
    EVENT_THREAD_END(p, w);
    EVENT_PARCEL_RESEND(p->id, p->action, p->size, p->target);
    EVENT_THREAD_RESUME(q, self);
    p->ustack = NULL;
    parcel_launch(p);
    break;

   case HPX_LCO_ERROR:
    dbg_error("interrupt returned LCO error %s.\n", hpx_strerror(e));

   case HPX_ERROR:
   default:
    dbg_error("interrupt produced unexpected error %s.\n", hpx_strerror(e));
  }

  // Restore the appropriate interrupt mask, if we need to. If the parent had a
  // mask, then we restore that, otherwise we restore the default system mask.
  if (masked) {
    dbg_check(pthread_sigmask(SIG_SETMASK, &mask, NULL));
  }
  else if (stack->masked) {
    dbg_check(pthread_sigmask(SIG_SETMASK, &here->mask, NULL));
  }

  stack->masked = masked;
  stack->cont = cont;
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
/// @returns          The parcel @p but with a valid stack.
static hpx_parcel_t *_try_bind(worker_t *w, hpx_parcel_t *p) {
  if (p->ustack) {
    return p;
  }

  // try and get a stack from the freelist, otherwise allocate a new one
  ustack_t *stack = w->stacks;
  if (stack) {
    w->stacks = stack->next;
    --w->nstacks;
    thread_init(stack, p, worker_execute_thread, stack->size);
  }
  else {
    stack = thread_new(p, worker_execute_thread);
  }

  ustack_t *old = parcel_swap_stack(p, stack);
  if (old) {
    dbg_error("Replaced stack %p with %p in %p: this usually means two workers "
              "are trying to start a lightweight thread at the same time.\n",
              (void*)old, (void*)stack, (void*)p);
  }

  return p;
}

static chase_lev_ws_deque_t *_work(worker_t *worker) {
  int id = sync_load(&worker->work_id, SYNC_RELAXED);
  return &worker->queues[id].work;
}

static chase_lev_ws_deque_t *_yielded(worker_t *worker) {
  int id = sync_load(&worker->work_id, SYNC_RELAXED);
  return &worker->queues[1 - id].work;
}

static void _swap_epoch(worker_t *worker) {
  int id = sync_load(&worker->work_id, SYNC_RELAXED);
  sync_store(&worker->work_id, 1 - id, SYNC_RELAXED);
}

/// Process the next available parcel from our work queue in a lifo order.
static hpx_parcel_t *_schedule_lifo(worker_t *w) {
  hpx_parcel_t *p = sync_chase_lev_ws_deque_pop(_work(w));
  INST_IF (p) {
    EVENT_SCHED_POP_LIFO(p->id);
    EVENT_SCHED_WQSIZE(sync_chase_lev_ws_deque_size(
      &w->queues[sync_load(&w->work_id, SYNC_RELAXED)].work));
  }
  return p;
}

/// Steal a parcel from a worker with the given @p id.
static hpx_parcel_t *_steal_from(worker_t *w, int id) {
  worker_t *victim = scheduler_get_worker(here->sched, id);
  hpx_parcel_t *p = sync_chase_lev_ws_deque_steal(_work(victim));
  if (p) {
    w->last_victim = id;
  } else {
    w->last_victim = -1;
  }
  EVENT_SCHED_STEAL(p ? p->id : 0, victim->id);
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

/// The action that pushes half the work to a thief.
static HPX_ACTION_DECL(_push_half);
static int _push_half_handler(int src) {
  const int steal_half_threshold = 6;
  log_sched("received push half request from worker %d\n", src);
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
    scheduler_spawn_at(parcels, src);
  }
  EVENT_SCHED_STEAL(parcels ? parcels->id : 0, self->id);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, 0, _push_half, _push_half_handler, HPX_INT);

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

  p = action_new_parcel(_push_half, HPX_HERE, 0, 0, 1, &w->id);
  parcel_prepare(p);
  scheduler_spawn_at(p, cpu);
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

/// A null checkpoint continuation.
///
/// This continuation is used by the main pthread schedule operation. It is
/// special because we don't need to do anything special when we're using the
/// system stack and parcel.
///
/// @param      UNUSED1 The previous parcel.
/// @param      UNUSED2 The continuation environment.
static void _null(hpx_parcel_t *UNUSED1, void *UNUSED2) {
}

/// A checkpoint continuation that unlocks a lock.
///
/// This is used by threads that are blocking on condition variables.
///
/// @param            p The previous parcel.
/// @param          env The continuation environment, which is a tatas lock.
static void _unlock(hpx_parcel_t *to, void *lock) {
  sync_tatas_release(lock);
}

/// A checkpoint continuation that frees the previous thread.
///
/// @param            p The previous parcel.
/// @param       UNUSED The continuation environment.
static void _free(hpx_parcel_t *p, void *UNUSED) {
  _free_stack(p, self);
  parcel_delete(p);
}

/// A checkpoint continuation that resends the previous parcel.
///
/// @param            p The previous parcel.
/// @param       UNUSED The continuation environment.
static void _resend(hpx_parcel_t *p, void *UNUSED) {
  _free_stack(p, self);
  parcel_launch(p);
}

/// A checkpoint continuation that sends the previous parcel as mail.
///
/// @param            p The previous parcel.
/// @param       UNUSED The continuation environment.
static void _send_mail(hpx_parcel_t *p, void *worker) {
  worker_t *w = worker;
  log_sched("sending %p to worker %d\n", (void*)p, w->id);
  sync_two_lock_queue_enqueue(&w->inbox, p);
}

/// A checkpoint continuation that puts the previous thread into the yield
/// queue.
///
/// @param            p The previous parcel.
/// @param       UNUSED The continuation environment.
static void _yield(hpx_parcel_t *p, void *env) {
  worker_t *w = env;
  sync_chase_lev_ws_deque_push(_yielded(w), p);
  w->yielded = 0;
}

/// A transfer continuation that schedules a parcel for lifo work.
///
/// This continuation can change the local scheduling policy from help-first to
/// work first, and will be used directly whenever we need to push lifo work.
///
/// @param            p The parcel that we're going to push.
/// @param       worker The worker pointer.
static void _push_lifo(hpx_parcel_t *p, void *worker) {
  dbg_assert(p->target != HPX_NULL);
  dbg_assert(actions[p->action].handler != NULL);
  EVENT_SCHED_PUSH_LIFO(p->id);
  GAS_TRACE_ACCESS(p->src, here->rank, p->target, p->size);
  worker_t *w = worker;
  uint64_t size = sync_chase_lev_ws_deque_push(_work(w), p);
  if (w->work_first >= 0) {
    w->work_first = (here->sched->wf_threshold < size);
  }
}

/// Process a mail queue.
///
/// This processes all of the parcels in the mailbox of the worker, moving them
/// into the work queue of the designated worker.
///
/// @param            w The worker to process mail (should be self).
static void _handle_mail(worker_t *w) {
  hpx_parcel_t *parcels = NULL;
  hpx_parcel_t *p = NULL;
  while ((parcels = sync_two_lock_queue_dequeue(&w->inbox))) {
    while ((p = parcel_stack_pop(&parcels))) {
      EVENT_SCHED_MAIL(p->id);
      log_sched("got mail %p\n", p);
      _push_lifo(p, w);
    }
  }
}

/// The non-blocking schedule operation.
///
/// This will schedule new work relatively quickly, in order to avoid delaying
/// the execution of the user's continuation. If there is no local work we can
/// find quickly we'll transfer back to the main pthread stack and go through an
/// extended transfer time.
///
/// @param            f The continuation function.
/// @param          env The continuation environment.
static void _schedule_nb(void (*f)(hpx_parcel_t *, void*), void *env) {
  EVENT_SCHED_BEGIN();
  worker_t *w = self;
  _handle_mail(w);
  hpx_parcel_t *p = (worker_is_stopped()) ? w->system : _schedule_lifo(w);
  p = (p) ? _try_bind(w, p) : w->system;
  dbg_assert(p != w->current);
  _transfer(p, f, env, w);
  EVENT_SCHED_END(0, 0);
}

/// Probe and progress the network during scheduling.
///
/// This is used in the blocking scheduler loop in order to handle the network.
///
/// @param            w The current worker thread.
static void _schedule_network(worker_t *w) {
  // suppress work-first scheduling while we're inside the network.
  w->work_first = -1;
  network_progress(w->network, 0);
  hpx_parcel_t *stack = network_probe(w->network, 0);
  w->work_first = 0;

  hpx_parcel_t *p = NULL;
  while ((p = parcel_stack_pop(&stack))) {
    EVENT_PARCEL_RECV(p->id, p->action, p->size, p->src, p->target);
    _push_lifo(p, w);
  }
}

/// The blocking scheduling loop.
///
/// Selects a new lightweight thread to run and transfers to it. After the
/// transfer has occurred, i.e. the worker has switched stacks and replaced its
/// current parcel, but before returning to user code, the scheduler will
/// execute @p f, passing it the previous parcel and the @p env specified
/// there.
///
/// @param            f A transfer continuation.
/// @param          env The transfer continuation environment.
static void _schedule(void (*f)(hpx_parcel_t *, void*), void *env) {
  INST(int source = -1);
  INST(int spins = 0);
  hpx_parcel_t *p = NULL;
  worker_t *w = self;
  while (!worker_is_stopped()) {
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
      INST(source = LIFO);
      break;
    }

    // Swap our yield queue with our primary queue
    _swap_epoch(w);

    // Do some network stuff;
    _schedule_network(w);

    // Try and steal some work
    if ((p = _schedule_steal(w))) {
      INST(source = STEAL);
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

  // Don't transfer to the same parcel.
  if (p != w->current) {
    _transfer(p, f, env, w);
  }
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
  sync_store(&w->work_id, 0, SYNC_RELAXED);
  w->active      = true;
  w->profiler    = NULL;
  w->bst         = NULL;
  w->logs        = NULL;
  w->network     = here->net;

  sync_chase_lev_ws_deque_init(&w->queues[0].work, work_size);
  sync_chase_lev_ws_deque_init(&w->queues[1].work, work_size);
  sync_two_lock_queue_init(&w->inbox, NULL);
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

int worker_start(void) {
  EVENT_SCHED_BEGIN();
  worker_t *w = self;
  dbg_assert(w);

  // double-check this
  dbg_assert(((uintptr_t)w & (HPX_CACHELINE_SIZE - 1)) == 0);
  dbg_assert(((uintptr_t)&w->queues & (HPX_CACHELINE_SIZE - 1)) == 0);
  dbg_assert(((uintptr_t)&w->inbox & (HPX_CACHELINE_SIZE - 1))== 0);

  // make sure the system is initialized
  dbg_assert(here && here->config && here->net);

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

    _schedule(_null, NULL);
  }

  w->system = NULL;
  w->current = NULL;

  int code = sched->stopped;
  if (code != HPX_SUCCESS && here->rank == 0) {
    log_error("hpx_run epoch exited with a non-zero exit code: %d.\n", code);
  }

  EVENT_SCHED_END(0, 0);
  return code;
}

// Spawn a parcel on a specified worker thread.
void scheduler_spawn_at(hpx_parcel_t *p, int thread) {
  dbg_assert(thread >= 0);
  dbg_assert(thread < here->sched->n_workers);

  worker_t *w = scheduler_get_worker(here->sched, thread);
  dbg_assert(w);
  dbg_assert(p);
  _send_mail(p, w);
}

// Spawn a parcel.
// This complicated function does a bunch of logic to figure out the proper
// method of computation for the parcel.
void scheduler_spawn(hpx_parcel_t *p) {
  worker_t *w = self;
  dbg_assert(w);
  dbg_assert(w->id >= 0);
  dbg_assert(p);
  dbg_assert(actions[p->action].handler != NULL);

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
  if (action_is_interrupt(p->action)) {
    _execute_interrupt(p);
    return;
  }

  // If we are running an interrupt, then we can't work-first since we don't
  // have our own stack to suspend.
  if (action_is_interrupt(current->action)) {
    _push_lifo(p, w);
    return;
  }

  // Process p work-first. If we're running the system thread then we need to
  // prevent it from being stolen, which we can do by using the NULL
  // continuation.
  EVENT_THREAD_SUSPEND(current, w);
  _transfer(_try_bind(w, p), (current == w->system) ? _null : _push_lifo, w, w);
  EVENT_THREAD_RESUME(current, self);
}

void scheduler_yield(void) {
  worker_t *w = self;
  dbg_assert(action_is_default(w->current->action));
  EVENT_SCHED_YIELD();
  self->yielded = true;
  EVENT_THREAD_SUSPEND(w->current, w);
  _schedule_nb(_yield, w);
  EVENT_THREAD_RESUME(w->current, self);
}

hpx_status_t scheduler_wait(tatas_lock_t *lock, cvar_t *condition) {
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
  _schedule_nb(_unlock, (void*)lock);
  EVENT_THREAD_RESUME(p, self);

  // reacquire the lco lock before returning
  sync_tatas_acquire(lock);
  return cvar_get_error(condition);
}

/// Resume list of parcels.
///
/// This scans through the passed list of parcels, and tests to see if it
/// corresponds to a thread (i.e., has a stack) or not. If its a thread, we
/// check to see if it has soft affinity and ship it to its home through a
/// mailbox if necessary. If it's just a parcel, we use the launch
/// infrastructure to send it off.
///
/// @param      parcels A stack of parcels to resume.
static void _resume_parcels(hpx_parcel_t *parcels) {
  hpx_parcel_t *p;
  while ((p = parcel_stack_pop(&parcels))) {
    ustack_t *stack = p->ustack;
    if (stack && stack->affinity >= 0) {
      scheduler_spawn_at(p, stack->affinity);
    } else {
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

void HPX_NORETURN worker_finish_thread(hpx_parcel_t *p, int status) {
  switch (status) {
   case HPX_RESEND:
    EVENT_THREAD_END(p, self);
    EVENT_PARCEL_RESEND(self->current->id, self->current->action,
                        self->current->size, self->current->src);
    _schedule_nb(_resend, NULL);

   case HPX_SUCCESS:
    if (!p->ustack->cont) {
      _vcontinue_parcel(p, 0, NULL);
    }
    EVENT_THREAD_END(p, self);
    _schedule_nb(_free, NULL);

   case HPX_LCO_ERROR:
    // rewrite to lco_error and continue the error status
    p->c_action = lco_error;
    _hpx_thread_continue(2, &status, sizeof(status));
    EVENT_THREAD_END(p, self);
    _schedule_nb(_free, NULL);

   case HPX_ERROR:
   default:
    dbg_error("thread produced unexpected error %s.\n", hpx_strerror(status));
  }

  unreachable();
}

int _hpx_thread_continue(int n, ...) {
  worker_t *w = self;
  hpx_parcel_t *p = w->current;

  va_list args;
  va_start(args, n);
  _vcontinue_parcel(p, n, &args);
  va_end(args);

  return HPX_SUCCESS;
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

const struct hpx_parcel *hpx_thread_current_parcel(void) {
  worker_t *w = self;
  return (w) ? w->current : NULL;
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
  worker_t *w = scheduler_get_worker(here->sched, affinity);
  EVENT_THREAD_SUSPEND(p, worker);
  _schedule_nb(_send_mail, w);
  EVENT_THREAD_RESUME(p, self);
}

void scheduler_suspend(void (*f)(hpx_parcel_t *, void*), void *env) {
  worker_t *w = self;
  hpx_parcel_t *p = w->current;
  log_sched("suspending %p in %s\n", (void*)self->current, actions[p->action].key);
  EVENT_THREAD_SUSPEND(self->current, self);
  _schedule_nb(f, env);
  EVENT_THREAD_RESUME(p, self);
  log_sched("resuming %p in %s\n", (void*)p, actions[p->action].key);
  (void)p;
}

