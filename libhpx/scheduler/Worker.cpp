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
#include "Condition.h"
#include "Thread.h"
#include "PriorityWorker.h"
#include "WorkstealingWorker.h"
#include "lco/LCO.h"
#include "libhpx/debug.h"
#include "libhpx/events.h"
#include "libhpx/libhpx.h"
#include "libhpx/memory.h"
#include "libhpx/system.h"
#include <cstring>
#ifdef HAVE_URCU
# include <urcu-qsbr.h>
#endif

namespace {
using libhpx::self;
using libhpx::WorkerBase;
using namespace libhpx::scheduler;
}

/// Storage for the thread-local worker pointer.
__thread WorkerBase * volatile libhpx::self;

WorkerBase::WorkerBase(Scheduler& sched, int id)
    : id_(id),
      sched_(sched),
      profiler_(nullptr),
      bst(nullptr),
      system_(nullptr),
      current_(nullptr),
      threads_(nullptr),
      lock_(),
      running_(),
      state_(STOP),
      inbox_(),
      thread_([this]() { enter(); })
{
  assert((uintptr_t)&inbox_ % HPX_CACHELINE_SIZE == 0);
}

WorkerBase::~WorkerBase() {
  thread_.join();
  while (auto* thread = threads_) {
    threads_ = thread->next;
    delete thread;
  }
}

void
WorkerBase::bind(hpx_parcel_t *p)
{
  dbg_assert(!p->thread);

  Thread* thread;
  if (void* buffer = threads_) {
    threads_ = threads_->next;
    thread = new(buffer) Thread(p, ExecuteUserThread);
  }
  else {
    thread = new Thread(p, ExecuteUserThread);
  }

  if (Thread* old = parcel_set_thread(p, thread)) {
    dbg_error("Replaced stack %p with %p in %p: cthis usually means two workers "
              "are trying to start a lightweight thread at the same time.\n",
              old, thread, p);
  }
}

void
WorkerBase::unbind(hpx_parcel_t* p)
{
  if (Thread* thread = parcel_set_thread(p, nullptr)) {
    thread->~Thread();
    threads_ = new(thread) FreelistNode(threads_);
  }
  else {
    return;
  }

  const auto limit = here->config->sched_stackcachelimit;
  if (limit < 0 || threads_->depth < limit) {
    return;
  }

  for (auto i = 0, e = util::ceil_div(limit, 2); i < e; ++i) {
    auto* thread = threads_->next;
    delete threads_;
    threads_ = thread;
  }
  assert(!threads_ || threads_->depth == util::ceil_div(limit, 2));
}

void
WorkerBase::enter()
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
  parcel_init(0, 0, 0, 0, 0, nullptr, 0, &system);
  Thread thread(&system);
  parcel_set_thread(&system, &thread);

  system_ = &system;
  current_ = &system;

  // At this point (once we have a "current_" pointer we can do any
  // architecture-specific initialization necessary, up to and including calling
  // ContextSwitch.
  Thread::InitArch(this);

  // Hang out here until we're shut down.
  while (state_ != SHUTDOWN) {
    run();                                   // returns when state_ != RUN
    sleep();                                 // returns when state_ != STOP
  }

  system_ = NULL;
  current_ = NULL;

#ifdef HAVE_APEX
  // finish whatever the last thing we were doing was
  if (profiler_) {
    apex_stop(profiler_);
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
WorkerBase::sleep()
{
  std::unique_lock<std::mutex> _(lock_);
  while (state_ == STOP) {
    onSleep();

    if (hpx_parcel_t *p = handleMail()) {
      spawn(p);
    }

    // go back to sleep
    sched_.subActive();
    running_.wait(_);
    sched_.addActive();
  }
}

void
WorkerBase::checkpoint(hpx_parcel_t *p, Continuation& f, void *sp)
{
  current_->thread->setSp(sp);
  std::swap(current_, p);
  f(p);

#ifdef HAVE_URCU
  rcu_quiescent_state();
#endif
}

void
WorkerBase::Checkpoint(hpx_parcel_t* p, Continuation& f, WorkerBase* w,
                       void *sp)
{
  w->checkpoint(p, f, sp);
}

void
WorkerBase::transfer(hpx_parcel_t *p, Continuation& f)
{
  dbg_assert(p != current_);

  if (p->thread == nullptr) {
    bind(p);
  }

  if (unlikely(current_->thread->getMasked())) {
    sigset_t set;
    dbg_check(pthread_sigmask(SIG_SETMASK, &here->mask, &set));
    ContextSwitch(p, f, this);
    dbg_check(pthread_sigmask(SIG_SETMASK, &set, NULL));
  }
  else {
    ContextSwitch(p, f, this);
  }
}

void
WorkerBase::run()
{
  auto null = [](hpx_parcel_t*){};
  while (state_ ==  RUN) {
    if (auto p = handleMail()) {
      transfer(p, null);
    }
    else if (auto p = onSchedule()) {
      transfer(p, null);
    }
    else if (auto p = sched_.schedule()) {
      transfer(p, null);
    }
    else if (auto p = onBalance()) {
      transfer(p, null);
    }
    else {
      sched_.kick();
#ifdef HAVE_URCU
      rcu_quiescent_state();
#endif
    }
  }
}

void
WorkerBase::ExecuteUserThread(hpx_parcel_t *p)
{
  WorkerBase* w = self;
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
    p->thread->invokeContinue();
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

void
WorkerBase::yield()
{
  dbg_assert(action_is_default(current_->action));
  EVENT_SCHED_YIELD();
  EVENT_THREAD_SUSPEND(current_);
  schedule([this](hpx_parcel_t* p) {
      pushYield(p);
    });

  // `this` is volatile across the scheduler call but we can't actually indicate
  // that, so re-read self here
  self->EVENT_THREAD_RESUME(current_);
}

void
WorkerBase::suspend(void (*f)(hpx_parcel_t *, void*), void *env)
{
  hpx_parcel_t* p = current_;
  log_sched("suspending %p in %s\n", p, actions[p->action].key);
  EVENT_THREAD_SUSPEND(p);
  schedule(std::bind(f, std::placeholders::_1, env));

  // `this` is volatile across the scheduler call but we can't actually indicate
  // that, so re-read self here
  self->EVENT_THREAD_RESUME(p);
  log_sched("resuming %p in %s\n", p, actions[p->action].key);
}

hpx_status_t
WorkerBase::wait(LCO& lco, Condition& cond)
{
  hpx_parcel_t* p = current_;
  // we had better be holding a lock here
  dbg_assert(p->thread->inLCO());

  if (hpx_status_t status = cond.push(p)) {
    return status;
  }

  EVENT_THREAD_SUSPEND(p);
  schedule([&lco](hpx_parcel_t* p) {
      lco.unlock(p);
    });

  // `this` is volatile across schedule
  self->EVENT_THREAD_RESUME(p);
  lco.lock(p);
  return cond.getError();
}

WorkerBase::FreelistNode::FreelistNode(FreelistNode* n)
    : next(n),
      depth((n) ? n->depth + 1 : 1)
{
}

void
WorkerBase::FreelistNode::operator delete(void* obj)
{
  Thread::operator delete(obj);
}

void
WorkerBase::schedule(Continuation& f)
{
  EVENT_SCHED_BEGIN();
  if (state_ != RUN) {
    transfer(system_, f);
  }
  else if (auto p = handleMail()) {
    transfer(p, f);
  }
  else if (hpx_parcel_t *p = onSchedule()) {
    transfer(p, f);
  }
  else {
    transfer(system_, f);
  }
  EVENT_SCHED_END(0, 0);
}

void
WorkerBase::spawn(hpx_parcel_t* p)
{
  dbg_assert(p);
  dbg_assert(actions[p->action].handler != NULL);

  // If we're not running then push the parcel through the global scheduler.
  if (state_ != RUN) {
    sched_.spawn(p);
  }
  else {
    onSpawn(p);
  }
}

hpx_parcel_t*
WorkerBase::handleMail()
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
      spawn(prev);
      prev = next;
    }
  } while ((parcels = inbox_.dequeue()));
  dbg_assert(prev);
  return prev;
}

WorkerBase*
WorkerBase::Create(const config_t *cfg, Scheduler& sched, int i)
{
  if (cfg->sched == HPX_SCHED_DEFAULT) {
    return new WorkstealingWorker(sched, i);
  }
  else if (cfg->sched == HPX_SCHED_WORKSTEALING) {
    return new WorkstealingWorker(sched, i);
  }
  else {
    return new PriorityWorker(sched, i);
  }
}
