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

#include "libhpx/Worker.h"
#include "thread.h"
#include "lco/LCO.h"
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/events.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/memory.h"
#include "libhpx/c_network.h"
#include "libhpx/Scheduler.h"
#include "libhpx/topology.h"
#include <cstring>
#ifdef HAVE_URCU
# include <urcu-qsbr.h>
#endif

namespace {
using libhpx::Worker;
using libhpx::self;
LIBHPX_ACTION(HPX_INTERRUPT, 0, StealHalf, Worker::StealHalfHandler,
              HPX_POINTER);
}

/// Storage for the thread-local worker pointer.
thread_local Worker * volatile libhpx::self;

Worker::Worker(int id)
    : id_(id),
      numaNode_(here->topology->cpu_to_numa[id % here->topology->ncpus]),
      seed_(id),
      workFirst_(0),
      nstacks_(0),
      lastVictim_(nullptr),
      profiler_(nullptr),
      bst(nullptr),
      logs(nullptr),
      stats(nullptr),
      system_(nullptr),
      current_(nullptr),
      stacks_(nullptr),
      lock_(),
      running_(),
      state_(STOP),
      workId_(0),
      queues_(),
      inbox_(),
      thread_([this]() { enter(); })
{
}

Worker::~Worker() {
  thread_.join();
  if (hpx_parcel_t* p = handleMail()) {
    parcel_delete(p);
  }

  while (hpx_parcel_t* p = popLIFO()) {
    parcel_delete(p);
  }

  while (ustack_t* stack = stacks_) {
    stacks_ = stack->next;
    thread_delete(stack);
  }
}

void
Worker::pushLIFO(hpx_parcel_t* p)
{
  dbg_assert(p->target != HPX_NULL);
  dbg_assert(actions[p->action].handler != NULL);
  EVENT_SCHED_PUSH_LIFO(p->id);
#if defined(HAVE_AGAS) && defined(HAVE_REBALANCING)
  rebalancer_add_entry(p->src, here->rank, p->target, p->size);
#elif defined(ENABLE_INSTRUMENTATION)
  EVENT_GAS_ACCESS(p->src, here->rank, p->target, p->size);
#endif
  uint64_t size = queues_[workId_].push(p);
  if (workFirst_ >= 0) {
    workFirst_ = (here->config->sched_wfthreshold < size);
  }
}

hpx_parcel_t*
Worker::popLIFO()
{
  hpx_parcel_t *p = queues_[workId_].pop();
  dbg_assert(!p || p != current_);
  INST_IF (p) {
    EVENT_SCHED_POP_LIFO(p->id);
    EVENT_SCHED_WQSIZE(queues_[workId_].size());
  }
  return p;
}

hpx_parcel_t *
Worker::handleNetwork()
{
  // don't do work first scheduling in the network
  int wf = workFirst_;
  workFirst_ = -1;
  network_progress(here->net, 0);

  hpx_parcel_t *stack = network_probe(here->net, 0);
  workFirst_ = wf;

  while (hpx_parcel_t *p = parcel_stack_pop(&stack)) {
    pushLIFO(p);
  }

  return popLIFO();
}

hpx_parcel_t*
Worker::handleMail()
{
  hpx_parcel_t *parcels = inbox_.dequeue();
  if (!parcels) {
    return NULL;
  }

  hpx_parcel_t *prev = parcel_stack_pop(&parcels);
  do {
    hpx_parcel_t *next = NULL;
    while ((next = parcel_stack_pop(&parcels))) {
      dbg_assert(next != current_);
      EVENT_SCHED_MAIL(prev->id);
      log_sched("got mail %p\n", prev);
      pushLIFO(prev);
      prev = next;
    }
  } while ((parcels = inbox_.dequeue()));
  dbg_assert(prev);
  return prev;
}

void
Worker::bind(hpx_parcel_t *p)
{
  // try and get a stack from the freelist, otherwise allocate a new one
  ustack_t *stack = stacks_;
  if (stack) {
    stacks_ = stack->next;
    --nstacks_;
    thread_init(stack, p, ExecuteUserThread, stack->size);
  }
  else {
    stack = thread_new(p, ExecuteUserThread);
  }

  ustack_t *old = parcel_swap_stack(p, stack);
  if (old) {
    dbg_error("Replaced stack %p with %p in %p: cthis usually means two workers "
              "are trying to start a lightweight thread at the same time.\n",
              (void*)old, (void*)stack, (void*)p);
  }
}

void
Worker::unbind(hpx_parcel_t* p)
{
  ustack_t *stack = parcel_swap_stack(p, NULL);
  if (!stack) {
    return;
  }

  stack->next = stacks_;
  stacks_ = stack;
  int32_t count = ++nstacks_;
  int32_t limit = here->config->sched_stackcachelimit;
  if (limit < 0 || count <= limit) {
    return;
  }

  int32_t half = ceil_div_32(limit, 2);
  log_sched("flushing half of the stack freelist (%d)\n", half);
  while (count > half) {
    stack = stacks_;
    stacks_ = stack->next;
    count = --nstacks_;
    thread_delete(stack);
  }
}

void
Worker::schedule(Continuation& f)
{
  EVENT_SCHED_BEGIN();
  if (state_ != RUN) {
    transfer(system_, f);
  }
  else if (hpx_parcel_t *p = handleMail()) {
    transfer(p, f);
  }
  else if (hpx_parcel_t *p = popLIFO()) {
    transfer(p, f);
  }
  else {
    transfer(system_, f);
  }
  EVENT_SCHED_END(0, 0);
}

void
Worker::enter()
{
  EVENT_SCHED_BEGIN();
  dbg_assert(here && here->config && here->gas && here->net);

  self = this;

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
  int status = system_set_worker_affinity(id_, policy);
  if (status != LIBHPX_OK) {
    log_dflt("WARNING: running with no worker thread affinity. "
             "This MAY result in diminished performance.\n");
  }

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

  system_ = &system;
  current_ = &system;

  // Hang out here until we're shut down.
  while (state_ != SHUTDOWN) {
    run();                                   // returns when state_ != RUN
    sleep();                                 // returns when state_ != STOP
  }

  system_ = NULL;
  current_ = NULL;

#ifdef HAVE_APEX
  // finish whatever the last thing we were doing was
  if (profiler) {
    apex_stop(profiler);
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
}

void
Worker::sleep()
{
  std::unique_lock<std::mutex> _(lock_);
  while (state_ == STOP) {
    while (hpx_parcel_t *p = queues_[1 - workId_].pop()) {
      pushLIFO(p);
    }

    if (hpx_parcel_t *p = handleMail()) {
      pushLIFO(p);
    }

    // go back to sleep
    Scheduler* sched = here->sched;
    sync_addf(&sched->n_active, -1, SYNC_ACQ_REL);
    running_.wait(_);
    sync_addf(&sched->n_active, 1, SYNC_ACQ_REL);
  }
}

void
Worker::checkpoint(hpx_parcel_t *p, Continuation& f, void *sp)
{
  current_->ustack->sp = sp;
  std::swap(current_, p);
  f(p);

#ifdef HAVE_URCU
  rcu_quiescent_state();
#endif
}

void
Worker::Checkpoint(hpx_parcel_t* p, Continuation& f, Worker* w, void *sp)
{
  w->checkpoint(p, f, sp);
}

void
Worker::transfer(hpx_parcel_t *p, Continuation& f)
{
  dbg_assert(p != current_);

  if (p->ustack == nullptr) {
    bind(p);
  }

  if (!current_->ustack->masked) {
    ContextSwitch(p, f, this);
  }
  else {
    sigset_t set;
    dbg_check(pthread_sigmask(SIG_SETMASK, &here->mask, &set));
    ContextSwitch(p, f, this);
    dbg_check(pthread_sigmask(SIG_SETMASK, &set, NULL));
  }
}

void
Worker::run()
{
  std::function<void(hpx_parcel_t*)> null([](hpx_parcel_t*){});
  while (state_ ==  RUN) {
    if (hpx_parcel_t *p = handleMail()) {
      transfer(p, null);
    }
    else if (hpx_parcel_t *p = popLIFO()) {
      transfer(p, null);
    }
    else if (hpx_parcel_t *p = handleEpoch()) {
      transfer(p, null);
    }
    else if (hpx_parcel_t *p = handleNetwork()) {
      transfer(p, null);
    }
    else if (hpx_parcel_t *p = handleSteal()) {
      transfer(p, null);
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
  if (0 <= affinity && affinity != id_) {
    dbg_assert(affinity < here->sched->n_workers);
    here->sched->workers[affinity]->pushMail(p);
    return;
  }

  // If we're not running then push the parcel and return. This prevents an
  // infinite spawn from inhibiting termination.
  if (state_ != RUN) {
    pushLIFO(p);
    return;
  }

  // If we're not in work-first mode, then push the parcel for later.
  if (workFirst_ < 1) {
    pushLIFO(p);
    return;
  }

  // If we're holding a lock then we have to push the spawn for later
  // processing, or we could end up causing a deadlock.
  if (current_->ustack->lco_depth) {
    pushLIFO(p);
    return;
  }

  // If we are currently running an interrupt, then we can't work-first since we
  // don't have our own stack to suspend.
  if (action_is_interrupt(current_->action)) {
    pushLIFO(p);
    return;
  }

  // Process p work-first. If we're running the system thread then we need to
  // prevent it from being stolen, which we can do by using the NULL
  // continuation.
  EVENT_THREAD_SUSPEND(current_);
  if (current_ == system_) {
    transfer(p, [](hpx_parcel_t*) {});
  }
  else {
    transfer(p, [this](hpx_parcel_t* p) { pushLIFO(p); });
  }
  self->EVENT_THREAD_RESUME(current_);          // re-read self
}

int
Worker::StealHalfHandler(Worker* src)
{
  if (hpx_parcel_t* half = self->stealHalf()) {
    src->pushMail(half);
  }
  return HPX_SUCCESS;
}

hpx_parcel_t*
Worker::stealFrom(Worker* victim) {
  hpx_parcel_t *p = victim->queues_[victim->workId_].steal();
  lastVictim_ = (p) ? victim : nullptr;
  EVENT_SCHED_STEAL((p) ? p->id : 0, victim->getId());
  return p;
}

hpx_parcel_t*
Worker::stealRandom()
{
  int n = here->sched->n_workers;
  int id;
  do {
    id = rand(n);
  } while (id == id_);
  return stealFrom(here->sched->workers[id]);
}

hpx_parcel_t*
Worker::stealRandomNode()
{
  int   n = here->topology->cpus_per_node;
  int cpu = rand(n);
  int  id = here->topology->numa_to_cpus[numaNode_][cpu];
  while (id == id_) {
    cpu = rand(n);
    id = here->topology->numa_to_cpus[numaNode_][cpu];
  }
  return stealFrom(here->sched->workers[id]);
}

hpx_parcel_t*
Worker::stealHalf()
{
  int qsize = queues_[workId_].size();
  if (qsize < MAGIC_STEAL_HALF_THRESHOLD) {
    return nullptr;
  }

  hpx_parcel_t *parcels = nullptr;
  for (int i = 0, e = qsize / 2; i < e; ++i) {
    hpx_parcel_t *p = popLIFO();
    if (!p) {
      break;
    }
    if (p->action == StealHalf) {
      parcel_delete(p);
      continue;
    }
    parcel_stack_push(&parcels, p);
  }
  return parcels;
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
hpx_parcel_t*
Worker::stealHierarchical()
{
  // disable hierarchical stealing if the worker threads are not
  // bound, or if the system is not hierarchical.
  if (here->config->thread_affinity == HPX_THREAD_AFFINITY_NONE) {
    return stealRandom();
  }

  if (here->topology->numa_to_cpus == NULL) {
    return stealRandom();
  }

  dbg_assert(numaNode_ >= 0);

  // step 1
  if (lastVictim_) {
    if (hpx_parcel_t* p = stealFrom(lastVictim_)) {
      return p;
    }
  }

  // step 2
  if (hpx_parcel_t* p = stealRandomNode()) {
    return p;
  }

  // step 3
  if (hpx_parcel_t* p = stealRandomNode()) {
    return p;
  }

  // step 4
  int nn = numaNode_;
  while (nn == numaNode_) {
    nn = rand(here->topology->nnodes);
  }

  int        idx = rand(here->topology->cpus_per_node);
  int        cpu = here->topology->numa_to_cpus[nn][idx];
  Worker* victim = here->sched->workers[cpu];
  Worker*    src = this;
  hpx_parcel_t* p = action_new_parcel(StealHalf, // action
                                      HPX_HERE,  // target
                                      0,         // c_action
                                      0,         // c_taget
                                      1,         // n args
                                      &src);     // reply
  parcel_prepare(p);
  victim->pushMail(p);
  return NULL;
}

hpx_parcel_t*
Worker::handleSteal()
{
  if (here->sched->n_workers == 1) {
    return NULL;
  }

  libhpx_sched_policy_t policy = here->config->sched_policy;
  switch (policy) {
    default:
      log_dflt("invalid scheduling policy, defaulting to random..");
    case HPX_SCHED_POLICY_DEFAULT:
    case HPX_SCHED_POLICY_RANDOM:
     return stealRandom();
    case HPX_SCHED_POLICY_HIER:
     return stealHierarchical();
  }
}

void
Worker::ExecuteUserThread(hpx_parcel_t *p)
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
    EVENT_PARCEL_RESEND(w->current_->id, w->current_->action,
                        w->current_->size, w->current_->src);
    w->schedule([w](hpx_parcel_t* p) {
        dbg_assert(w == self);
        w->unbind(p);
        parcel_launch(p);
      });
    unreachable();

   case HPX_SUCCESS:
    thread_continue_va(p->ustack, 0, NULL);
    w = self;
    w->EVENT_THREAD_END(p);
    w->schedule([w](hpx_parcel_t* p) {
        dbg_assert(w == self);
        w->unbind(p);
        parcel_delete(p);
      });
    unreachable();

   case HPX_LCO_ERROR:
    // rewrite to lco_error and continue the error status
    p->c_action = lco_error;
    _hpx_thread_continue(2, &status, sizeof(status));
    w = self;
    w->EVENT_THREAD_END(p);
    w->schedule([w](hpx_parcel_t* p) {
        dbg_assert(w == self);
        w->unbind(p);
        parcel_delete(p);
      });
    unreachable();

   case HPX_ERROR:
   default:
    dbg_error("thread produced unexpected error %s.\n", hpx_strerror(status));
  }
}
