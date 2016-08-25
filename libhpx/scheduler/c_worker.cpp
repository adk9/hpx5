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
  Worker* w = self;
  hpx_parcel_t *old = w->current;
  w->current = to;
  old->ustack->sp = sp;
  _checkpoint_env_t *c = static_cast<_checkpoint_env_t *>(env);
  c->f(old, c->env);

#ifdef HAVE_URCU
  // do cthis in the checkpoint continuation to minimize time holding LCO locks
  // (released in c->f if necessary)
  rcu_quiescent_state();
#endif
}

void
Worker::transfer(hpx_parcel_t *p, void (*f)(hpx_parcel_t *, void *), void *e)
{
  dbg_assert(p != current);

  if (p->ustack == nullptr) {
    bind(p);
  }

  if (!this->current->ustack->masked) {
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

/// Steal a parcel from a worker with the given @p id.
static hpx_parcel_t *_steal_from(Worker *cthis, int id) {
  Worker& victim = cthis->sched_.workers[id];
  hpx_parcel_t *p = victim.queues[victim.work_id].steal();
  cthis->last_victim = (p) ? id : -1;
  EVENT_SCHED_STEAL(p ? p->id : 0, victim.id);
  return p;
}

/// Steal a parcel from a random worker out of all workers.
static hpx_parcel_t *_steal_random_all(Worker *cthis) {
  int n = cthis->sched_.n_workers;
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
  int qsize = cthis->queues[cthis->work_id].size();
  if (qsize < steal_half_threshold) {
    return HPX_SUCCESS;
  }

  // pop half of the total parcels from my work queue
  int count = (qsize >> 1);
  hpx_parcel_t *parcels = NULL;
  for (int i = 0; i < count; ++i) {
    hpx_parcel_t *p = cthis->popLIFO();
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
hpx_parcel_t*
Worker::handleSteal()
{
  int n = this->sched_.n_workers;
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
/// @param       worker The worker (avoids TLS hit).
static void _free(hpx_parcel_t *p, void *worker) {
  dbg_assert(worker == self);
  static_cast<Worker*>(worker)->unbind(p);
  parcel_delete(p);
}

/// A checkpoint continuation that resends the previous parcel.
///
/// @param            p The previous parcel.
/// @param       worker The worker (avoids TLS hit).
static void _resend(hpx_parcel_t *p, void* worker) {
  dbg_assert(worker == self);
  static_cast<Worker*>(worker)->unbind(p);
  parcel_launch(p);
}

/// A checkpoint continuation that puts the previous thread into the yield
/// queue.
///
/// @param            p The previous parcel.
/// @param       worker The current worker.
static void _yield(hpx_parcel_t *p, void *worker) {
  dbg_assert(worker == self);
  static_cast<Worker*>(worker)->pushYield(p);
  self->yielded = false;
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

void
Worker::schedule()
{
  while (state ==  SCHED_RUN) {
    if (hpx_parcel_t *p = handleMail()) {
      transfer(p, _null, 0);
    }
    else if (hpx_parcel_t *p = popLIFO()) {
      transfer(p, _null, 0);
    }
    else if (hpx_parcel_t *p = handleEpoch()) {
      transfer(p, _null, 0);
    }
    else if (hpx_parcel_t *p = handleNetwork()) {
      transfer(p, _null, 0);
    }
    else if (hpx_parcel_t *p = handleSteal()) {
      transfer(p, _null, 0);
    }
    else {
#ifdef HAVE_URCU
      rcu_quiescent_state();
#endif
    }
  }
}

void
Worker::spawn(hpx_parcel_t* p)
{
  dbg_assert(p);
  dbg_assert(actions[p->action].handler != NULL);

  // If the target has affinity then send the parcel to that worker.
  int affinity = gas_get_affinity(here->gas, p->target);
  if (0 <= affinity && affinity != id) {
    scheduler_spawn_at(p, affinity);
    return;
  }

  // If we're not running then push the parcel and return. This prevents an
  // infinite spawn from inhibiting termination.
  if (state != SCHED_RUN) {
    pushLIFO(p);
    return;
  }

  // If we're not in work-first mode, then push the parcel for later.
  if (work_first < 1) {
    pushLIFO(p);
    return;
  }

  // If we're holding a lock then we have to push the spawn for later
  // processing, or we could end up causing a deadlock.
  if (current->ustack->lco_depth) {
    pushLIFO(p);
    return;
  }

  // If we are currently running an interrupt, then we can't work-first since we
  // don't have our own stack to suspend.
  if (action_is_interrupt(current->action)) {
    pushLIFO(p);
    return;
  }

  // Process p work-first. If we're running the system thread then we need to
  // prevent it from being stolen, which we can do by using the NULL
  // continuation.
  EVENT_THREAD_SUSPEND(current);
  transfer(p, (current == system_) ? _null : _push_lifo, this);
  self->EVENT_THREAD_RESUME(current);
}

void
scheduler_yield(void)
{
  Worker *w = self;
  dbg_assert(action_is_default(w->current->action));
  EVENT_SCHED_YIELD();
  self->yielded = true;
  w->EVENT_THREAD_SUSPEND(w->current);
  w->schedule(_yield, w);
  self->EVENT_THREAD_RESUME(w->current);
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

  w->EVENT_THREAD_SUSPEND(p);
  w->schedule(_unlock, lco);
  self->EVENT_THREAD_RESUME(p);

  // reacquire the lco lock before returning
  static_cast<LCO*>(lco)->lock(p);
  return condition->getError();
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

void
Worker::Execute(hpx_parcel_t *p)
{
  Worker* w = self;
  w->EVENT_THREAD_RUN(p);
  EVENT_SCHED_END(0, 0);
  int status = HPX_SUCCESS;
  try {
    status = action_exec_parcel(p->action, p);
  } catch (const int &nonLocal) {
    status = nonLocal;
  }

  // NB: No EVENT_SCHED_BEGIN here. All code paths from this point will reach
  //     _schedule_nb in worker.c and that will begin scheduling
  //     again. Effectively we consider continuation generation as user-level
  //     work.
  switch (status) {
   case HPX_RESEND:
    w = self;
    w->EVENT_THREAD_END(p);
    EVENT_PARCEL_RESEND(w->current->id, w->current->action,
                        w->current->size, w->current->src);
    w->schedule(_resend, w);
    unreachable();

   case HPX_SUCCESS:
    if (!p->ustack->cont) {
      _vcontinue_parcel(p, 0, NULL);
    }
    w = self;
    w->EVENT_THREAD_END(p);
    w->schedule(_free, w);
    unreachable();

   case HPX_LCO_ERROR:
    // rewrite to lco_error and continue the error status
    p->c_action = lco_error;
    _hpx_thread_continue(2, &status, sizeof(status));
    w = self;
    w->EVENT_THREAD_END(p);
    w->schedule(_free, w);
    unreachable();

   case HPX_ERROR:
   default:
    dbg_error("thread produced unexpected error %s.\n", hpx_strerror(status));
  }
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

int hpx_thread_get_tls_id(void) {
  Worker *w = self;
  ustack_t *stack = w->current->ustack;
  if (stack->tls_id < 0) {
    stack->tls_id = sync_fadd(&w->sched_.next_tls_id, 1, SYNC_ACQ_REL);
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
  log_sched("suspending %p in %s\n", (void*)w->current, actions[p->action].key);
  w->EVENT_THREAD_SUSPEND(w->current);
  w->schedule(f, env);
  self->EVENT_THREAD_RESUME(p);
  log_sched("resuming %p in %s\n", (void*)p, actions[p->action].key);
  (void)p;
}
