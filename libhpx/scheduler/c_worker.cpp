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

#include "libhpx/Worker.h"
#include "events.h"
#include "thread.h"
#include "Condition.h"
#include "lco/LCO.h"
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/events.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/instrumentation.h>
#include <libhpx/memory.h>
#include <libhpx/c_network.h>
#include <libhpx/parcel.h>                      // used as thread-control block
#include <libhpx/process.h>
#include <libhpx/rebalancer.h>
#include <libhpx/c_scheduler.h>
#include "libhpx/Scheduler.h"
#include <libhpx/system.h>
#include <libhpx/termination.h>
#include <libhpx/topology.h>
#include <hpx/builtins.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_URCU
# include <urcu-qsbr.h>
#endif

namespace {
using libhpx::scheduler::Condition;
using libhpx::scheduler::LCO;
}

/// Storage for the thread-local worker pointer.
__thread Worker * volatile self = NULL;

/// Swap a worker's current parcel.
///
/// @param            p The parcel we're swapping in.
/// @param           sp The checkpointed stack pointer for the current parcel.
/// @param            w The current worker.
///
/// @returns           The previous parcel.
static hpx_parcel_t *_swap_current(hpx_parcel_t *p, void *sp, Worker *w) {
  hpx_parcel_t *q = w->current;
  w->current = p;
  q->ustack->sp = sp;
  return q;
}

/// Freelist a parcel's stack.
///
/// This will push the parcel's stack onto the local worker's freelist, and
/// possibly trigger a freelist flush if there are too many parcels cached
/// locally.
///
/// @param            p The parcel that has the stack that we are freeing.
/// @param            w The local worker.
static void _free_stack(hpx_parcel_t *p, Worker *w) {
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

/// Bind a new stack to a parcel, so that we can execute it as a thread.
///
/// The newly created thread is runnable, and can be thread_transfer()ed to in
/// the same way as any other lightweight thread can be. Calling _bind_stack()
/// on a parcel that already has a stack (i.e., a thread) is permissible and has
/// no effect.
///
/// @param          p The parcel that is generating cthis thread.
/// @param          w The current worker (we will use its stack freelist)
static void _bind_stack(hpx_parcel_t *p, Worker *w) {
  if (p->ustack) {
    return;
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
    dbg_error("Replaced stack %p with %p in %p: cthis usually means two workers "
              "are trying to start a lightweight thread at the same time.\n",
              (void*)old, (void*)stack, (void*)p);
  }
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
  _checkpoint_env_t *c = static_cast<_checkpoint_env_t *>(env);
  c->f(prev, c->env);

#ifdef HAVE_URCU
  // do cthis in the checkpoint continuation to minimize time holding LCO locks
  // (released in c->f if necessary)
  rcu_quiescent_state();
#endif
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
                      void *e, Worker *w) {
  _bind_stack(p, w);

  if (!w->current->ustack->masked) {
    _checkpoint_env_t env = { .f = f, .env = e };
    thread_transfer(p, _checkpoint, &env);
  }
  else {
    sigset_t set;
    dbg_check(pthread_sigmask(SIG_SETMASK, &here->mask, &set));
    _checkpoint_env_t env = { .f = f, .env = e };
    thread_transfer(p, _checkpoint, &env);
    dbg_check(pthread_sigmask(SIG_SETMASK, &set, NULL));
  }
}

hpx_parcel_t *_hpx_thread_generate_continuation(int n, ...) {
  hpx_parcel_t *p = self->current;

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

static chase_lev_ws_deque_t *_work(Worker *cthis) {
  return &cthis->queues[cthis->work_id].work;
}

static chase_lev_ws_deque_t *_yielded(Worker *cthis) {
  return &cthis->queues[1 - cthis->work_id].work;
}

static hpx_parcel_t *_pop_lifo(Worker *cthis) {
  return cthis->popLIFO();
}

/// Steal a parcel from a worker with the given @p id.
static hpx_parcel_t *_steal_from(Worker *cthis, int id) {
  Worker *victim = (Worker*)scheduler_get_worker(cthis->sched, id);
  void *buffer = sync_chase_lev_ws_deque_steal(_work(victim));
  hpx_parcel_t *p = (hpx_parcel_t*)buffer;
  cthis->last_victim = (p) ? id : -1;
  EVENT_SCHED_STEAL(p ? p->id : 0, victim->id);
  return p;
}

/// Steal a parcel from a random worker out of all workers.
static hpx_parcel_t *_steal_random_all(Worker *cthis) {
  int n = cthis->sched->n_workers;
  int id;
  do {
    id = rand_r(&cthis->seed) % n;
  } while (id == cthis->id);
  return _steal_from(cthis, id);
}

/// Steal a parcel from a random worker from the same NUMA node.
static hpx_parcel_t *_steal_random_node(Worker *cthis) {
  int n = here->topology->cpus_per_node;
  int id;
  do {
    int cpu = rand_r(&cthis->seed) % n;
    id = here->topology->numa_to_cpus[cthis->numa_node][cpu];
  } while (id == cthis->id);
  return _steal_from(cthis, id);
}

/// The action that pushes half the work to a thief.
static int _push_half_handler(int src);
static LIBHPX_ACTION(HPX_INTERRUPT, 0, _push_half, _push_half_handler, HPX_INT);
static int _push_half_handler(int src) {
  Worker *cthis = self;
  const int steal_half_threshold = 6;
  log_sched("received push half request from worker %d\n", src);
  int qsize = sync_chase_lev_ws_deque_size(_work(cthis));
  if (qsize < steal_half_threshold) {
    return HPX_SUCCESS;
  }

  // pop half of the total parcels from my work queue
  int count = (qsize >> 1);
  hpx_parcel_t *parcels = NULL;
  for (int i = 0; i < count; ++i) {
    hpx_parcel_t *p = _pop_lifo(cthis);
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
  EVENT_SCHED_STEAL(parcels ? parcels->id : 0, cthis->id);
  return HPX_SUCCESS;
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
static hpx_parcel_t *_steal_hier(Worker *cthis)
{
  // disable hierarchical stealing if the worker threads are not
  // bound, or if the system is not hierarchical.
  if (here->config->thread_affinity == HPX_THREAD_AFFINITY_NONE) {
    return _steal_random_all(cthis);
  }

  if (here->topology->numa_to_cpus == NULL) {
    return _steal_random_all(cthis);
  }

  dbg_assert(cthis->numa_node >= 0);
  hpx_parcel_t *p;

  // step 1
  int cpu = cthis->last_victim;
  if (cpu >= 0) {
    p = _steal_from(cthis, cpu);
    if (p) {
      return p;
    }
  }

  // step 2
  p = _steal_random_node(cthis);
  if (p) {
    return p;
  } else {
    // step 3
    p = _steal_random_node(cthis);
    if (p) {
      return p;
    }
  }

  // step 4
  int numa_node;
  do {
    numa_node = rand_r(&cthis->seed) % here->topology->nnodes;
  } while (numa_node == cthis->numa_node);

  int idx = rand_r(&cthis->seed) % here->topology->cpus_per_node;
  cpu = here->topology->numa_to_cpus[numa_node][idx];

  p = action_new_parcel(_push_half, HPX_HERE, 0, 0, 1, &cthis->id);
  parcel_prepare(p);
  scheduler_spawn_at(p, cpu);
  return NULL;
}

/// Steal a lightweight thread during scheduling.
///
/// NB: we can be much smarter about who to steal from and how much to
/// steal. Ultimately though, we're building a distributed runtime so SMP work
/// stealing isn't that big a deal.
static hpx_parcel_t *_handle_steal(Worker *cthis) {
  int n = cthis->sched->n_workers;
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
      p = _steal_random_all(cthis);
      break;
    case HPX_SCHED_POLICY_HIER:
      p = _steal_hier(cthis);
      break;
  }
  return p;
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

/// A checkpoint continuation that unlocks an LCO.
///
/// This is used by threads that are blocking on condition variables.
///
/// @param            p The previous parcel.
/// @param          env The continuation environment, which is an lco.
static void _unlock(hpx_parcel_t *p, void *lco) {
  static_cast<LCO*>(lco)->unlock(p);
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
/// @param       worker The worker we're sending mail to.
static void _send_mail(hpx_parcel_t *p, void *worker) {
  Worker *cthis = (Worker *)worker;
  log_sched("sending %p to worker %d\n", (void*)p, cthis->id);
  sync_two_lock_queue_enqueue(&cthis->inbox, p);
}

/// A checkpoint continuation that puts the previous thread into the yield
/// queue.
///
/// @param            p The previous parcel.
/// @param       worker The current worker.
static void _yield(hpx_parcel_t *p, void *worker) {
  Worker *cthis = (Worker *)worker;
  sync_chase_lev_ws_deque_push(_yielded(cthis), p);
  cthis->yielded = 0;
}

/// A transfer continuation that schedules a parcel for lifo work.
///
/// This continuation can change the local scheduling policy from help-first to
/// work first, and will be used directly whenever we need to push lifo work.
///
/// @param            p The parcel that we're going to push.
/// @param       worker The worker pointer.
static void _push_lifo(hpx_parcel_t *p, void *worker) {
  static_cast<Worker*>(worker)->pushLIFO(p);
}

static hpx_parcel_t *_handle_mail(Worker *cthis) {
  return cthis->handleMail();
}

/// Handle the network.
///
/// This will return a parcel from the network if it finds any. It will also
/// refill the local work queue.
///
/// @param         cthis The current worker.
///
/// @returns            A parcel from the network if there is one.
static hpx_parcel_t *_handle_network(Worker *cthis) {
  // don't do work first scheduling in the network
  int wf = cthis->work_first;
  cthis->work_first = -1;
  network_progress(cthis->network, 0);

  hpx_parcel_t *stack = network_probe(cthis->network, 0);
  cthis->work_first = wf;

  hpx_parcel_t *p = NULL;
  while ((p = parcel_stack_pop(&stack))) {
    _push_lifo(p, cthis);
  }
  return _pop_lifo(cthis);
}

/// Handle a scheduler worker epoch transition.
///
/// Currently we don't handle our yielded work eagerly---we'll try and prefer
/// network and stolen work to yield work.
static hpx_parcel_t *_handle_epoch(Worker *cthis) {
  cthis->work_id = 1 - cthis->work_id;
  return NULL;
}

/// Check to see if the worker is running.
static int _is_state(const Worker *w, int state) {
  return (sync_load(&w->state, SYNC_ACQUIRE) == state);
}

/// The non-blocking schedule operation.
///
/// This will schedule new work relatively quickly, in order to avoid delaying
/// the execution of the user's continuation. If there is no local work we can
/// find quickly we'll transfer back to the main pthread stack and go through an
/// extended transfer time.
///
/// @param         cthis The current worker.
/// @param            f The continuation function.
/// @param          env The continuation environment.
static void _schedule_nb(Worker *cthis, void (*f)(hpx_parcel_t *, void*),
                         void *env) {
  EVENT_SCHED_BEGIN();
  hpx_parcel_t *p = NULL;
  if (!_is_state(cthis, SCHED_RUN)) {
    p = cthis->system;
  }
  else if ((p = _handle_mail(cthis))) {
  }
  else if ((p = _pop_lifo(cthis))) {
  }
  else {
    p = cthis->system;
  }
  dbg_assert(p != cthis->current);
  _transfer(p, f, env, cthis);
  EVENT_SCHED_END(0, 0);
}

/// The primary schedule loop.
///
/// This will continue to try and schedule lightweight threads while the
/// worker's state is SCHED_RUN.
///
/// @param         cthis The current worker thread.
static void _schedule(Worker *cthis) {
  while (_is_state(cthis, SCHED_RUN)) {
    hpx_parcel_t *p;
    if ((p = _handle_mail(cthis))) {
      _transfer(p, _null, 0, cthis);
    }
    else if ((p = _pop_lifo(cthis))) {
      _transfer(p, _null, 0, cthis);
    }
    else if ((p = _handle_epoch(cthis))) {
      _transfer(p, _null, 0, cthis);
    }
    else if ((p = _handle_network(cthis))) {
      _transfer(p, _null, 0, cthis);
    }
    else if ((p = _handle_steal(cthis))) {
      _transfer(p, _null, 0, cthis);
    }
    else {
#ifdef HAVE_URCU
      rcu_quiescent_state();
#endif
    }
  }
}

/// The stop loop.
///
/// This will continue to deal with the stopped state while the worker's state
/// is SCHED_STOP. The current implementation does cthis using a condition
/// variable.
///
/// @param         cthis The current worker thread.
static void _stop(Worker *cthis) {
  pthread_mutex_lock(&cthis->lock);
  while (_is_state(cthis, SCHED_STOP)) {
    // make sure we don't have anything stuck in mail or yield
    void *buffer;
    while ((buffer = sync_chase_lev_ws_deque_pop(_yielded(cthis)))) {
      hpx_parcel_t *p = (hpx_parcel_t*)buffer;
      _push_lifo(p, cthis);
    }

    if ((buffer = _handle_mail(cthis))) {
      hpx_parcel_t *p = (hpx_parcel_t*)buffer;
      _push_lifo(p, cthis);
    }

    // go back to sleep
    sync_addf(&cthis->sched->n_active, -1, SYNC_ACQ_REL);
    pthread_cond_wait(&cthis->running, &cthis->lock);
    sync_addf(&cthis->sched->n_active, 1, SYNC_ACQ_REL);
  }
  pthread_mutex_unlock(&cthis->lock);
}

static void *_run(void *worker) {
  Worker *cthis = (Worker *)worker;
  EVENT_SCHED_BEGIN();
  dbg_assert(here && here->config && here->gas && here->net);
  dbg_assert(cthis);
  dbg_assert(((uintptr_t)cthis & (HPX_CACHELINE_SIZE - 1)) == 0);
  dbg_assert(((uintptr_t)&cthis->queues & (HPX_CACHELINE_SIZE - 1)) == 0);
  dbg_assert(((uintptr_t)&cthis->inbox & (HPX_CACHELINE_SIZE - 1))== 0);

  self = cthis;

  // Ensure that all of the threads have joined the address spaces.
  as_join(AS_REGISTERED);
  as_join(AS_GLOBAL);
  as_join(AS_CYCLIC);

#ifdef HAVE_URCU
  // Make ourself visible to urcu.
  rcu_register_thread();
#endif

#ifdef HAVE_APEX
  // let APEX know there is a new thread
  apex_register_thread("HPX WORKER THREAD");
#endif

  // affinitize the worker thread
  libhpx_thread_affinity_t policy = here->config->thread_affinity;
  int status = system_set_worker_affinity(cthis->id, policy);
  if (status != LIBHPX_OK) {
    log_dflt("WARNING: running with no worker thread affinity. "
             "This MAY result in diminished performance.\n");
  }

  int cpu = cthis->id % here->topology->ncpus;
  cthis->numa_node = here->topology->cpu_to_numa[cpu];

  // allocate a parcel and a stack header for the system stack
  hpx_parcel_t system;
  parcel_init(0, 0, 0, 0, 0, NULL, 0, &system);
  ustack_t stack = {
    .sp = NULL,
    .parcel = &system,
    .next = NULL,
    .lco_depth = 0,
    .tls_id = -1,
    .stack_id = -1,
    .size = 0
  };
  system.ustack = &stack;

  cthis->system = &system;
  cthis->current = cthis->system;
  cthis->work_first = 0;

  // Hang out here until we're shut down.
  while (!_is_state(cthis, SCHED_SHUTDOWN)) {
    _schedule(cthis);                         // returns when state != SCHED_RUN
    _stop(cthis);                             // returns when state != SCHED_STOP
  }

  cthis->system = NULL;
  cthis->current = NULL;

#ifdef HAVE_APEX
  // finish whatever the last thing we were doing was
  if (cthis->profiler) {
    apex_stop(cthis->profiler);
  }
  // let APEX know the thread is exiting
  apex_exit_thread();
#endif

#ifdef HAVE_URCU
  // leave the urcu domain
  rcu_unregister_thread();
#endif

  // leave the global address space
  as_leave();

  EVENT_SCHED_END(0, 0);
  return NULL;
}

int worker_create(Worker *cthis) {
  int e = pthread_create(&cthis->thread, NULL, _run, cthis);
  if (e) {
    dbg_error("cannot start worker thread %d (%s).\n", cthis->id, strerror(e));
    return LIBHPX_ERROR;
  }
  else {
    return LIBHPX_OK;
  }
}

void worker_join(Worker *cthis) {
  if (cthis->thread == 0) {
    log_dflt("worker_join called on thread (%d) that didn't start\n", cthis->id);
    return;
  }

  int e = pthread_join(cthis->thread, NULL);
  if (e) {
    log_error("cannot join worker thread %d (%s).\n", cthis->id, strerror(e));
  }
}

void worker_stop(Worker *w) {
  pthread_mutex_lock(&w->lock);
  sync_store(&w->state, SCHED_STOP, SYNC_RELEASE);
  pthread_mutex_unlock(&w->lock);
}

void worker_start(Worker *w) {
  pthread_mutex_lock(&w->lock);
  sync_store(&w->state, SCHED_RUN, SYNC_RELEASE);
  pthread_cond_broadcast(&w->running);
  pthread_mutex_unlock(&w->lock);
}

void worker_shutdown(Worker *w) {
  pthread_mutex_lock(&w->lock);
  sync_store(&w->state, SCHED_SHUTDOWN, SYNC_RELEASE);
  pthread_cond_broadcast(&w->running);
  pthread_mutex_unlock(&w->lock);
}

// Spawn a parcel on a specified worker thread.
void scheduler_spawn_at(hpx_parcel_t *p, int thread) {
  dbg_assert(thread >= 0);
  dbg_assert(here && here->sched && (here->sched->n_workers > thread));

  Worker *w = (Worker*)scheduler_get_worker(here->sched, thread);
  dbg_assert(w);
  dbg_assert(p);
  _send_mail(p, w);
}

// Spawn a parcel.
// This complicated function does a bunch of logic to figure out the proper
// method of computation for the parcel.
void scheduler_spawn(hpx_parcel_t *p) {
  Worker *w = self;
  dbg_assert(w);
  dbg_assert(w->id >= 0);
  dbg_assert(p);
  dbg_assert(actions[p->action].handler != NULL);

  // If the target has affinity then send the parcel to that worker.
  int affinity = gas_get_affinity(here->gas, p->target);
  if (0 <= affinity && affinity != w->id) {
    scheduler_spawn_at(p, affinity);
    return;
  }

  // If we're not running then push the parcel and return. This prevents an
  // infinite spawn from inhibiting termination.
  if (!_is_state(w, SCHED_RUN)) {
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

  // If we are currently running an interrupt, then we can't work-first since we
  // don't have our own stack to suspend.
  if (action_is_interrupt(current->action)) {
    _push_lifo(p, w);
    return;
  }

  // Process p work-first. If we're running the system thread then we need to
  // prevent it from being stolen, which we can do by using the NULL
  // continuation.
  EVENT_THREAD_SUSPEND(current, w);
  _transfer(p, (current == w->system) ? _null : _push_lifo, w, w);
  EVENT_THREAD_RESUME(current, self);
}

void scheduler_yield(void) {
  Worker *w = self;
  dbg_assert(action_is_default(w->current->action));
  EVENT_SCHED_YIELD();
  self->yielded = true;
  EVENT_THREAD_SUSPEND(w->current, w);
  _schedule_nb(w, _yield, w);
  EVENT_THREAD_RESUME(w->current, self);
}

hpx_status_t
scheduler_wait(void *lco, void *cond)
{
  Condition* condition = static_cast<Condition*>(cond);
  // push the current thread onto the condition variable---no lost-update
  // problem here because we're holing the @p lock
  Worker *w = self;
  hpx_parcel_t *p = w->current;

  // we had better be holding a lock here
  dbg_assert(p->ustack->lco_depth > 0);

  if (hpx_status_t status = condition->push(p)) {
    return status;
  }

  EVENT_THREAD_SUSPEND(p, w);
  _schedule_nb(w, _unlock, lco);
  EVENT_THREAD_RESUME(p, self);

  // reacquire the lco lock before returning
  static_cast<LCO*>(lco)->lock(p);
  return condition->getError();
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
    parcel_launch(p);
  }
}

void scheduler_signal(void *cond) {
  Condition* condition = static_cast<Condition*>(cond);
  _resume_parcels(condition->pop());
}

void scheduler_signal_all(void *cond) {
  Condition* condition = static_cast<Condition*>(cond);
  _resume_parcels(condition->popAll());
}

void scheduler_signal_error(void* cond, hpx_status_t code) {
  Condition* condition = static_cast<Condition*>(cond);
  _resume_parcels(condition->setError(code));
}

void HPX_NORETURN worker_finish_thread(hpx_parcel_t *p, int status) {
  switch (status) {
   case HPX_RESEND:
    EVENT_THREAD_END(p, self);
    EVENT_PARCEL_RESEND(self->current->id, self->current->action,
                        self->current->size, self->current->src);
    _schedule_nb(self, _resend, NULL);

   case HPX_SUCCESS:
    if (!p->ustack->cont) {
      _vcontinue_parcel(p, 0, NULL);
    }
    EVENT_THREAD_END(p, self);
    _schedule_nb(self, _free, NULL);

   case HPX_LCO_ERROR:
    // rewrite to lco_error and continue the error status
    p->c_action = lco_error;
    _hpx_thread_continue(2, &status, sizeof(status));
    EVENT_THREAD_END(p, self);
    _schedule_nb(self, _free, NULL);

   case HPX_ERROR:
   default:
    dbg_error("thread produced unexpected error %s.\n", hpx_strerror(status));
  }

  unreachable();
}

int _hpx_thread_continue(int n, ...) {
  Worker *w = self;
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
  Worker *w = self;
  return (w) ? w->id : -1;
}

const struct hpx_parcel *hpx_thread_current_parcel(void) {
  Worker *w = self;
  return (w) ? w->current : NULL;
}

hpx_addr_t hpx_thread_current_target(void) {
  Worker *w = self;
  return (w && w->current) ? w->current->target : HPX_NULL;
}

hpx_addr_t hpx_thread_current_cont_target(void) {
  Worker *w = self;
  return (w && w->current) ? w->current->c_target : HPX_NULL;
}

hpx_action_t hpx_thread_current_action(void) {
  Worker *w = self;
  return (w && w->current) ? w->current->action : HPX_ACTION_NULL;
}

hpx_action_t hpx_thread_current_cont_action(void) {
  Worker *w = self;
  return (w && w->current) ? w->current->c_action : HPX_ACTION_NULL;
}

hpx_pid_t hpx_thread_current_pid(void) {
  Worker *w = self;
  return (w && w->current) ? w->current->pid : HPX_NULL;
}

uint32_t hpx_thread_current_credit(void) {
  Worker *w = self;
  return (w && w->current) ? w->current->credit : 0;
}

int hpx_thread_get_tls_id(void) {
  Worker *w = self;
  ustack_t *stack = w->current->ustack;
  if (stack->tls_id < 0) {
    stack->tls_id = sync_fadd(&w->sched->next_tls_id, 1, SYNC_ACQ_REL);
  }

  return stack->tls_id;
}

intptr_t hpx_thread_can_alloca(size_t bytes) {
  ustack_t *current = self->current->ustack;
  return (uintptr_t)&current - (uintptr_t)current->stack - bytes;
}

void scheduler_suspend(void (*f)(hpx_parcel_t *, void*), void *env) {
  Worker *w = self;
  hpx_parcel_t *p = w->current;
  log_sched("suspending %p in %s\n", (void*)self->current, actions[p->action].key);
  EVENT_THREAD_SUSPEND(self->current, self);
  _schedule_nb(w, f, env);
  EVENT_THREAD_RESUME(p, self);
  log_sched("resuming %p in %s\n", (void*)p, actions[p->action].key);
  (void)p;
}

int hpx_is_active(void) {
  return (self->current != NULL);
}
