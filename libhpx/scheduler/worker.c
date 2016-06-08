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

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
  STEAL,
  SYSTEM,
  NETWORK,
  MAIL,
  EPOCH
} TRANSFER_SOURCES;

/// Swap a worker's current parcel.
///
/// @param            p The parcel we're swapping in.
/// @param           sp The checkpointed stack pointer for the current parcel.
/// @param            w The current worker.
///
/// @returns           The previous parcel.
static hpx_parcel_t *_swap_current(hpx_parcel_t *p, void *sp, worker_t *w) {
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

/// Bind a new stack to a parcel, so that we can execute it as a thread.
///
/// The newly created thread is runnable, and can be thread_transfer()ed to in
/// the same way as any other lightweight thread can be. Calling _bind_stack()
/// on a parcel that already has a stack (i.e., a thread) is permissible and has
/// no effect.
///
/// @param          p The parcel that is generating this thread.
/// @param          w The current worker (we will use its stack freelist)
static void _bind_stack(hpx_parcel_t *p, worker_t *w) {
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
    dbg_error("Replaced stack %p with %p in %p: this usually means two workers "
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
  _bind_stack(p, w);

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

static chase_lev_ws_deque_t *_work(worker_t *this) {
  int id = sync_load(&this->work_id, SYNC_RELAXED);
  return &this->queues[id].work;
}

static chase_lev_ws_deque_t *_yielded(worker_t *this) {
  int id = sync_load(&this->work_id, SYNC_RELAXED);
  return &this->queues[1 - id].work;
}

/// Process the next available parcel from our work queue in a lifo order.
static hpx_parcel_t *_pop_lifo(worker_t *this) {
  chase_lev_ws_deque_t *work = _work(this);
  hpx_parcel_t *p = sync_chase_lev_ws_deque_pop(work);
  INST_IF (p) {
    EVENT_SCHED_POP_LIFO(p->id);
    EVENT_SCHED_WQSIZE(sync_chase_lev_ws_deque_size(work));
  }
  return p;
}

/// Steal a parcel from a worker with the given @p id.
static hpx_parcel_t *_steal_from(worker_t *this, int id) {
  worker_t *victim = scheduler_get_worker(this->sched, id);
  hpx_parcel_t *p = sync_chase_lev_ws_deque_steal(_work(victim));
  this->last_victim = (p) ? id : -1;
  EVENT_SCHED_STEAL(p ? p->id : 0, victim->id);
  return p;
}

/// Steal a parcel from a random worker out of all workers.
static hpx_parcel_t *_steal_random_all(worker_t *this) {
  int n = this->sched->n_workers;
  int id;
  do {
    id = rand_r(&this->seed) % n;
  } while (id == this->id);
  return _steal_from(this, id);
}

/// Steal a parcel from a random worker from the same NUMA node.
static hpx_parcel_t *_steal_random_node(worker_t *this) {
  int n = here->topology->cpus_per_node;
  int id;
  do {
    int cpu = rand_r(&this->seed) % n;
    id = here->topology->numa_to_cpus[this->numa_node][cpu];
  } while (id == this->id);
  return _steal_from(this, id);
}

/// The action that pushes half the work to a thief.
static HPX_ACTION_DECL(_push_half);
static int _push_half_handler(int src) {
  worker_t *this = self;
  const int steal_half_threshold = 6;
  log_sched("received push half request from worker %d\n", src);
  int qsize = sync_chase_lev_ws_deque_size(_work(this));
  if (qsize < steal_half_threshold) {
    return HPX_SUCCESS;
  }

  // pop half of the total parcels from my work queue
  int count = (qsize >> 1);
  hpx_parcel_t *parcels = NULL;
  for (int i = 0; i < count; ++i) {
    hpx_parcel_t *p = _pop_lifo(this);
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
  EVENT_SCHED_STEAL(parcels ? parcels->id : 0, this->id);
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
static hpx_parcel_t *_steal_hier(worker_t *this) {

  // disable hierarchical stealing if the worker threads are not
  // bound, or if the system is not hierarchical.
  libhpx_thread_affinity_t policy = here->config->thread_affinity;
  if (unlikely(policy == HPX_THREAD_AFFINITY_NONE ||
               here->topology->numa_to_cpus == NULL)) {
    return _steal_random_all(this);
  }

  dbg_assert(this->numa_node >= 0);
  hpx_parcel_t *p;

  // step 1
  int cpu = this->last_victim;
  if (cpu >= 0) {
    p = _steal_from(this, cpu);
    if (p) {
      return p;
    }
  }

  // step 2
  p = _steal_random_node(this);
  if (p) {
    return p;
  } else {
    // step 3
    p = _steal_random_node(this);
    if (p) {
      return p;
    }
  }

  // step 4
  int numa_node;
  do {
    numa_node = rand_r(&this->seed) % here->topology->nnodes;
  } while (numa_node == this->numa_node);

  int idx = rand_r(&this->seed) % here->topology->cpus_per_node;
  cpu = here->topology->numa_to_cpus[numa_node][idx];

  p = action_new_parcel(_push_half, HPX_HERE, 0, 0, 1, &this->id);
  parcel_prepare(p);
  scheduler_spawn_at(p, cpu);
  return NULL;
}

/// Steal a lightweight thread during scheduling.
///
/// NB: we can be much smarter about who to steal from and how much to
/// steal. Ultimately though, we're building a distributed runtime so SMP work
/// stealing isn't that big a deal.
static hpx_parcel_t *_handle_steal(worker_t *this) {
  int n = this->sched->n_workers;
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
      p = _steal_random_all(this);
      break;
    case HPX_SCHED_POLICY_HIER:
      p = _steal_hier(this);
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
/// @param       worker The worker we're sending mail to.
static void _send_mail(hpx_parcel_t *p, void *worker) {
  worker_t *this = worker;
  log_sched("sending %p to worker %d\n", (void*)p, this->id);
  sync_two_lock_queue_enqueue(&this->inbox, p);
}

/// A checkpoint continuation that puts the previous thread into the yield
/// queue.
///
/// @param            p The previous parcel.
/// @param       worker The current worker.
static void _yield(hpx_parcel_t *p, void *worker) {
  worker_t *this = worker;
  sync_chase_lev_ws_deque_push(_yielded(this), p);
  this->yielded = 0;
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
  worker_t *this = worker;
  uint64_t size = sync_chase_lev_ws_deque_push(_work(this), p);
  if (this->work_first >= 0) {
    this->work_first = (here->config->sched_wfthreshold < size);
  }
}

/// Process a mail queue.
///
/// This processes all of the parcels in the mailbox of the worker, moving them
/// into the work queue of the designated worker. It will return a parcel if
/// there was one.
///
/// @param         this The worker to process mail.
///
/// @returns            A parcel from the mailbox if there is one.
static hpx_parcel_t *_handle_mail(worker_t *this) {
  hpx_parcel_t *parcels = NULL;
  hpx_parcel_t *p = NULL;
  while ((parcels = sync_two_lock_queue_dequeue(&this->inbox))) {
    while ((p = parcel_stack_pop(&parcels))) {
      EVENT_SCHED_MAIL(p->id);
      log_sched("got mail %p\n", p);
      _push_lifo(p, this);
    }
  }
  return _pop_lifo(this);
}

/// Handle the network.
///
/// This will return a parcel from the network if it finds any. It will also
/// refill the local work queue.
///
/// @param         this The current worker.
///
/// @returns            A parcel from the network if there is one.
static hpx_parcel_t *_handle_network(worker_t *this) {
  // don't do work first scheduling in the network
  int wf = this->work_first;
  this->work_first = -1;
  network_progress(this->network, 0);

  hpx_parcel_t *stack = network_probe(this->network, 0);
  this->work_first = wf;

  hpx_parcel_t *p = NULL;
  while ((p = parcel_stack_pop(&stack))) {
    EVENT_PARCEL_RECV(p->id, p->action, p->size, p->src, p->target);
    _push_lifo(p, this);
  }
  return _pop_lifo(this);
}

/// Handle a scheduler worker epoch transition.
///
/// Currently we don't handle our yielded work eagerly---we'll try and prefer
/// network and stolen work to yield work.
static hpx_parcel_t *_handle_epoch(worker_t *this) {
  int id = sync_load(&this->work_id, SYNC_RELAXED);
  sync_store(&this->work_id, 1 - id, SYNC_RELAXED);
  return NULL;
}

/// Check to see if the worker is running.
static int _is_running(const worker_t *w) {
  return (sync_load(&w->state, SYNC_ACQUIRE) == SCHED_RUN);
}

/// Check to see if the worker is running.
static int _is_stopped(const worker_t *w) {
  return (sync_load(&w->state, SYNC_ACQUIRE) == SCHED_STOP);
}

/// Check to see if the worker is shutdown.
static int _is_shutdown(const worker_t *w) {
  return (sync_load(&w->state, SYNC_ACQUIRE) == SCHED_SHUTDOWN);
}

/// The non-blocking schedule operation.
///
/// This will schedule new work relatively quickly, in order to avoid delaying
/// the execution of the user's continuation. If there is no local work we can
/// find quickly we'll transfer back to the main pthread stack and go through an
/// extended transfer time.
///
/// @param         this The current worker.
/// @param            f The continuation function.
/// @param          env The continuation environment.
static void _schedule_nb(worker_t *this, void (*f)(hpx_parcel_t *, void*),
                         void *env) {
  EVENT_SCHED_BEGIN();
  hpx_parcel_t *p = NULL;
  if (!_is_running(this)) {
    p = this->system;
  }
  else if ((p = _handle_mail(this))) {
  }
  else if ((p = _pop_lifo(this))) {
  }
  else {
    p = this->system;
  }
  dbg_assert(p != this->current);
  _transfer(p, f, env, this);
  EVENT_SCHED_END(0, 0);
}

/// The primary schedule loop.
///
/// This will continue to try and schedule lightweight threads while the
/// worker's state is SCHED_RUN.
///
/// @param         this The current worker thread.
static void _schedule(worker_t *this) {
  while (_is_running(this)) {
    hpx_parcel_t *p;
    if ((p = _handle_mail(this))) {
    }
    else if ((p = _pop_lifo(this))) {
    }
    else if ((p = _handle_epoch(this))) {
    }
    else if ((p = _handle_network(this))) {
    }
    else if ((p = _handle_steal(this))) {
    }
    else {
      continue;
    }
    _transfer(p, _null, 0, this);
  }
}

/// The stop loop.
///
/// This will continue to deal with the stopped state while the worker's state
/// is SCHED_STOP. The current implementation does this using a condition
/// variable.
///
/// @param         this The current worker thread.
static void _stop(worker_t *this) {
  pthread_mutex_lock(&this->lock);
  while (_is_stopped(this)) {
    // make sure we don't have anything stuck in mail or yield
    hpx_parcel_t *p;
    while ((p = sync_chase_lev_ws_deque_pop(_yielded(this)))) {
      _push_lifo(p, this);
    }

    if ((p = _handle_mail(this))) {
      _push_lifo(p, this);
    }

    sync_addf(&this->sched->n_active, -1, SYNC_ACQ_REL);
    pthread_cond_wait(&this->running, &this->lock);
    sync_addf(&this->sched->n_active, 1, SYNC_ACQ_REL);
  }
  pthread_mutex_unlock(&this->lock);
}

static void *_run(void *worker) {
  worker_t *this = worker;
  EVENT_SCHED_BEGIN();
  dbg_assert(here && here->config && here->gas && here->net);
  dbg_assert(this);
  dbg_assert(((uintptr_t)this & (HPX_CACHELINE_SIZE - 1)) == 0);
  dbg_assert(((uintptr_t)&this->queues & (HPX_CACHELINE_SIZE - 1)) == 0);
  dbg_assert(((uintptr_t)&this->inbox & (HPX_CACHELINE_SIZE - 1))== 0);

  self = this;

  // Ensure that all of the threads have joined the address spaces.
  as_join(AS_REGISTERED);
  as_join(AS_GLOBAL);
  as_join(AS_CYCLIC);

#ifdef HAVE_APEX
  // let APEX know there is a new thread
  apex_register_thread("HPX WORKER THREAD");
#endif

  // affinitize the worker thread
  libhpx_thread_affinity_t policy = here->config->thread_affinity;
  int status = system_set_worker_affinity(this->id, policy);
  if (status != LIBHPX_OK) {
    log_dflt("WARNING: running with no worker thread affinity. "
             "This MAY result in diminished performance.\n");
  }

  int cpu = this->id % here->topology->ncpus;
  this->numa_node = here->topology->cpu_to_numa[cpu];

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

  this->system = &system;
  this->current = this->system;
  this->work_first = 0;

  // The worker threads hang out inside this loop until they are shut down. They
  // will continuously schedule until their run state is not SCHED_RUN, and they
  // will stay in the stop state until their state is not SCHED_STOP.
  while (!_is_shutdown(this)) {
    _schedule(this);
    _stop(this);
  }

  this->system = NULL;
  this->current = NULL;

#ifdef HAVE_APEX
  // let APEX know the thread is exiting
  apex_exit_thread();
#endif

  // leave the global address space
  as_leave();

  EVENT_SCHED_END(0, 0);
  return NULL;
}

void worker_init(worker_t *this, struct scheduler *sched, int id) {
  this->thread      = 0;
  this->id          = id;
  this->seed        = id;
  this->work_first  = 0;
  this->nstacks     = 0;
  this->yielded     = 0;
  this->active      = 1;
  this->last_victim = -1;
  this->numa_node   = -1;
  this->profiler    = NULL;
  this->bst         = NULL;
  this->network     = here->net;
  this->logs        = NULL;
  this->sched       = sched;
  this->system      = NULL;
  this->current     = NULL;
  this->stacks      = NULL;

  if (pthread_mutex_init(&this->lock, NULL)) {
    dbg_error("could not initialize the lock for %d\n", id);
  }
  if (pthread_cond_init(&this->running, NULL)) {
    dbg_error("could not initialize the condition for %d\n", id);
  }
  sync_store(&this->work_id, 0, SYNC_RELAXED);
  sync_store(&this->state, SCHED_STOP, SYNC_RELAXED);

  sync_chase_lev_ws_deque_init(&this->queues[0].work, 32);
  sync_chase_lev_ws_deque_init(&this->queues[1].work, 32);
  sync_two_lock_queue_init(&this->inbox, NULL);
}

void worker_fini(worker_t *this) {
  if (pthread_cond_destroy(&this->running)) {
    dbg_error("Failed to destroy the running condition for %d\n", self->id);
  }
  if (pthread_mutex_destroy(&this->lock)) {
    dbg_error("Failed to destroy the lock for %d\n", self->id);
  }

  hpx_parcel_t *p = NULL;
  // clean up the mailbox
  if ((p = _handle_mail(this))) {
    parcel_delete(p);
  }
  sync_two_lock_queue_fini(&this->inbox);

  // and clean up the workqueue parcels
  while ((p = _pop_lifo(this))) {
    parcel_delete(p);
  }
  sync_chase_lev_ws_deque_fini(&this->queues[0].work);
  sync_chase_lev_ws_deque_fini(&this->queues[1].work);

  // and delete any cached stacks
  ustack_t *stack = NULL;
  while ((stack = this->stacks)) {
    this->stacks = stack->next;
    thread_delete(stack);
  }
}

int worker_create(worker_t *this) {
  int e = pthread_create(&this->thread, NULL, _run, this);
  if (e) {
    dbg_error("cannot start worker thread %d (%s).\n", this->id, strerror(e));
    return LIBHPX_ERROR;
  }
  else {
    return LIBHPX_OK;
  }
}

void worker_join(worker_t *this) {
  if (this->thread == 0) {
    log_dflt("worker_join called on thread (%d) that didn't start\n", this->id);
    return;
  }

  int e = pthread_join(this->thread, NULL);
  if (e) {
    log_error("cannot join worker thread %d (%s).\n", this->id, strerror(e));
  }
}

void worker_stop(worker_t *w) {
  pthread_mutex_lock(&w->lock);
  sync_store(&w->state, SCHED_STOP, SYNC_RELEASE);
  pthread_mutex_unlock(&w->lock);
}

void worker_start(worker_t *w) {
  pthread_mutex_lock(&w->lock);
  sync_store(&w->state, SCHED_RUN, SYNC_RELEASE);
  pthread_cond_broadcast(&w->running);
  pthread_mutex_unlock(&w->lock);
}

void worker_shutdown(worker_t *w) {
  pthread_mutex_lock(&w->lock);
  sync_store(&w->state, SCHED_SHUTDOWN, SYNC_RELEASE);
  pthread_cond_broadcast(&w->running);
  pthread_mutex_unlock(&w->lock);
}

// Spawn a parcel on a specified worker thread.
void scheduler_spawn_at(hpx_parcel_t *p, int thread) {
  dbg_assert(thread >= 0);
  dbg_assert(here && here->sched && (here->sched->n_workers > thread));

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

  // If we're not running then push the parcel and return. This prevents an
  // infinite spawn from inhibiting termination.
  if (!_is_running(w)) {
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
  worker_t *w = self;
  dbg_assert(action_is_default(w->current->action));
  EVENT_SCHED_YIELD();
  self->yielded = true;
  EVENT_THREAD_SUSPEND(w->current, w);
  _schedule_nb(w, _yield, w);
  EVENT_THREAD_RESUME(w->current, self);
}

hpx_status_t scheduler_wait(void *lock, cvar_t *condition) {
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
  _schedule_nb(w, _unlock, lock);
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
    parcel_launch(p);
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
  worker_t *w = self;
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
  worker_t *w = self;
  hpx_parcel_t *p = w->current;
  log_sched("suspending %p in %s\n", (void*)self->current, actions[p->action].key);
  EVENT_THREAD_SUSPEND(self->current, self);
  _schedule_nb(w, f, env);
  EVENT_THREAD_RESUME(p, self);
  log_sched("resuming %p in %s\n", (void*)p, actions[p->action].key);
  (void)p;
}

