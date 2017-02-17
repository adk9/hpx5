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

#include "WorkstealingWorker.h"
#include "Thread.h"
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/events.h"
#include "libhpx/Network.h"                     // progress
#include "libhpx/parcel.h"
#include "libhpx/Topology.h"
#include "libhpx/util/Random.h"

using namespace libhpx;
using namespace libhpx::scheduler;
static LIBHPX_ACTION(HPX_INTERRUPT, 0, StealHalf,
                     WorkstealingWorker::StealHalfHandler,
                     HPX_POINTER);

WorkstealingWorker::WorkstealingWorker(Scheduler& sched, int id)
    : WorkerBase(sched, id),
      util::Aligned<HPX_CACHELINE_SIZE>(),
      numaNode_(here->topology->cpu_to_numa[id % here->topology->ncpus]),
      rng_(util::getRNG()),
      cpu_(0, sched.getNWorkers() - 1),
      node_(0, here->topology->nnodes - 1),
      cpuOnNode_(0, here->topology->cpus_per_node - 1),
      workFirst_(0),
      lastVictim_(nullptr),
      work_()
{
  assert((uintptr_t)&work_ % HPX_CACHELINE_SIZE == 0);
}

WorkstealingWorker::~WorkstealingWorker()
{
  while (hpx_parcel_t* p = popLIFO()) {
    parcel_delete(p);
  }
}

void
WorkstealingWorker::pushLIFO(hpx_parcel_t* p)
{
  dbg_assert(p->target != HPX_NULL);
  dbg_assert(actions[p->action].handler != NULL);
  EVENT_SCHED_PUSH_LIFO(p->id);
#if defined(HAVE_AGAS) && defined(HAVE_REBALANCING)
  rebalancer_add_entry(p->src, here->rank, p->target, p->size);
#elif defined(ENABLE_INSTRUMENTATION)
  EVENT_GAS_ACCESS(p->src, here->rank, p->target, p->size);
#endif
  uint64_t size = work_.push(p);
  if (workFirst_ >= 0) {
    workFirst_ = (here->config->sched_wfthreshold < size);
  }
}

hpx_parcel_t*
WorkstealingWorker::popLIFO()
{
  hpx_parcel_t *p = work_.pop();
  dbg_assert(!p || p != current_);
  INST_IF (p) {
    EVENT_SCHED_POP_LIFO(p->id);
    EVENT_SCHED_WQSIZE(work_.size());
  }
  return p;
}

void
WorkstealingWorker::onSpawn(hpx_parcel_t* p)
{
  // If the target has affinity then send the parcel to that worker.
  int affinity = here->gas->getAffinity(p->target);
  if (0 <= affinity && affinity != id_) {
    GetWorker(affinity)->pushMail(p);
    return;
  }

  // If we're not in work-first mode, then push the parcel for later.
  if (workFirst_ < 1) {
    pushLIFO(p);
    return;
  }

  // If we're holding a lock then we have to push the spawn for later
  // processing, or we could end up causing a deadlock.
  if (current_->thread->inLCO()) {
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
WorkstealingWorker::StealHalfHandler(WorkstealingWorker* src)
{
  auto w = static_cast<WorkstealingWorker*>(self);
  if (hpx_parcel_t* half = w->stealHalf()) {
    src->pushMail(half);
  }
  return HPX_SUCCESS;
}

hpx_parcel_t*
WorkstealingWorker::stealFrom(WorkstealingWorker* victim) {
  hpx_parcel_t *p = victim->work_.steal();
  lastVictim_ = (p) ? victim : nullptr;
  EVENT_SCHED_STEAL((p) ? p->id : 0, victim->getId());
  return p;
}

hpx_parcel_t*
WorkstealingWorker::stealRandom()
{
  while (true) {
    int cpu = cpu_(rng_);
    if (cpu != id_) {
      return stealFrom(GetWorker(cpu));
    }
  }
}

hpx_parcel_t*
WorkstealingWorker::stealRandomNode()
{
  while (true) {
    int cpu = cpuOnNode_(rng_);
    int id = here->topology->numa_to_cpus[numaNode_][cpu];
    if (id != id_) {
      return stealFrom(GetWorker(id));
    }
  }
}

hpx_parcel_t*
WorkstealingWorker::stealHalf()
{
  int qsize = work_.size();
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
WorkstealingWorker::stealHierarchical()
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
    nn = node_(rng_);
  }

  int      idx = cpuOnNode_(rng_);
  int      cpu = here->topology->numa_to_cpus[nn][idx];
  auto* victim = GetWorker(cpu);
  auto*    src = this;
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
WorkstealingWorker::handleSteal()
{
  if (here->sched->getNWorkers() == 1) {
    return NULL;
  }

  libhpx_sched_steal_policy_t policy = here->config->sched_policy;
  switch (policy) {
   default:
    log_dflt("invalid scheduling policy, defaulting to random..");
   case HPX_SCHED_STEAL_POLICY_DEFAULT:
   case HPX_SCHED_STEAL_POLICY_RANDOM:
    return stealRandom();
   case HPX_SCHED_STEAL_POLICY_HIER:
    return stealHierarchical();
  }
}

WorkstealingWorker*
WorkstealingWorker::GetWorker(int id)
{
  return static_cast<WorkstealingWorker*>(here->sched->getWorker(id));
}
