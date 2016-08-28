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

#include "libhpx/Scheduler.h"
#include "libhpx/c_scheduler.h"
#include "Condition.h"
#include "Thread.h"
#include "lco/LCO.h"
#include "libhpx/action.h"
#include "libhpx/config.h"
#include "libhpx/debug.h"
#include "libhpx/gas.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/memory.h"
#include "libhpx/c_network.h"
#include "libhpx/process.h"
#include "libhpx/rebalancer.h"
#include "hpx/builtins.h"
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

namespace {
using libhpx::Worker;
using libhpx::self;
using libhpx::scheduler::Condition;
using libhpx::scheduler::LCO;
}

scheduler_t *
scheduler_new(const config_t *cfg)
{
  Thread::SetStackSize(cfg->stacksize);

  const int workers = cfg->threads;
  Scheduler *sched = NULL;
  size_t bytes = sizeof(*sched) + workers * sizeof(Worker*);
  if (posix_memalign((void**)&sched, HPX_CACHELINE_SIZE, bytes)) {
    dbg_error("could not allocate a scheduler.\n");
    return NULL;
  }

  // This thread can allocate even though it's not a scheduler thread.
  as_join(AS_REGISTERED);
  as_join(AS_GLOBAL);
  as_join(AS_CYCLIC);

  pthread_mutex_init(&sched->lock, NULL);
  pthread_cond_init(&sched->stopped, NULL);
  sync_store(&sched->state, SCHED_STOP, SYNC_RELEASE);
  sync_store(&sched->next_tls_id, 0, SYNC_RELEASE);
  sync_store(&sched->code, HPX_SUCCESS, SYNC_RELEASE);
  sync_store(&sched->n_active, workers, SYNC_RELEASE);
  sched->n_workers = workers;
  sched->n_target = workers;
  sched->ns_wait = 100000000;
  sched->epoch = 0;
  sched->spmd = 0;
  sched->output = NULL;

  log_sched("initialized a new scheduler.\n");
  return sched;
}

void
scheduler_delete(void *obj)
{
  Scheduler *sched = static_cast<Scheduler*>(obj);
  for (int i = 0, e = sched->n_workers; i < e; ++i) {
    if (sched->workers[i]) {
      sched->workers[i]->shutdown();
      delete sched->workers[i];
    }
  }

  free(sched);
  as_leave();
}

void *
scheduler_get_worker(void *obj, int id)
{
  Scheduler *sched = static_cast<Scheduler*>(obj);
  assert(id >= 0);
  assert(id < sched->n_workers);
  return sched->workers[id];
}

static void
_wait(void * const scheduler)
{
  auto* csched = static_cast<Scheduler * const>(scheduler);
#ifdef HAVE_APEX
  int prev = csched->n_target;
  int n = min_int(apex_get_thread_cap(), csched->n_workers);
  log_sched("apex adjusting from %d to %d workers\n", prev, n);
  csched->n_target = n;
  auto op = (n < prev) ? Worker::stop : Worker::start;
  for (int i = max_int(prev, n), e = min_int(prev, n); i >= e; --i) {
    op(&csched->workers[i]);
  }

  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct timespec ts = {
    .tv_sec = tv.tv_sec + 0,
    .tv_nsec = csched->ns_wait                   // todo: be adaptive here
  };
  pthread_cond_timedwait(&csched->stopped, &csched->lock, &ts);
#else
  pthread_cond_wait(&csched->stopped, &csched->lock);
#endif
}

int
scheduler_start(void *obj, int spmd, hpx_action_t act, void *out, int n,
                va_list *args)
{
  Scheduler *sched = static_cast<Scheduler*>(obj);

  log_dflt("hpx started running %d\n", sched->epoch);

  // Create the worker threads for the first epoch.
  if (sched->epoch == 0) {
    for (int i = 0, e = sched->n_workers; i < e; ++i) {
      sched->workers[i] = new Worker(i);
    }
  }

  // remember the output slot
  sched->spmd = spmd;
  sched->output = out;

  if (spmd || here->rank == 0) {
    hpx_parcel_t *p = action_new_parcel_va(act, HPX_HERE, 0, 0, n, args);
    parcel_prepare(p);
    scheduler_spawn_at(p, 0);
  }

  // switch the state and then start all the workers
  sched->code = HPX_SUCCESS;
  sched->state = SCHED_RUN;
  for (int i = 0, e = sched->n_target; i < e; ++i) {
    sched->workers[i]->start();
  }

  // wait for someone to stop the scheduler
  pthread_mutex_lock(&sched->lock);
  while (sched->state == SCHED_RUN) {
    _wait(sched);
  }
  pthread_mutex_unlock(&sched->lock);

  // stop all of the worker threads
  for (int i = 0, e = sched->n_target; i < e; ++i) {
    sched->workers[i]->stop();
  }

  // Use sched crude barrier to wait for the worker threads to stop.
  while (sync_load(&sched->n_active, SYNC_ACQUIRE)) {
  }

  // return the exit code
  DEBUG_IF (sched->code != HPX_SUCCESS && here->rank == 0) {
    log_error("hpx_run epoch exited with exit code (%d).\n", sched->code);
  }
  log_dflt("hpx stopped running %d\n", sched->epoch);

  // clear the output slot
  sched->spmd = 0;
  sched->output = NULL;

  // bump the epoch
  sched->epoch++;
  return sched->code;
}

void
scheduler_set_output(void *obj, size_t bytes, const void *out)
{
  Scheduler *sched = static_cast<Scheduler*>(obj);
  if (!bytes) return;
  if (!sched->output) return;
  pthread_mutex_lock(&sched->lock);
  memcpy(sched->output, out, bytes);
  pthread_mutex_unlock(&sched->lock);
}

static int
_scheduler_set_output_async_handler(const void *out, size_t bytes)
{
  scheduler_set_output(here->sched, bytes, out);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _scheduler_set_output_async,
                     _scheduler_set_output_async_handler, HPX_POINTER,
                     HPX_SIZE_T);

void
scheduler_stop(void *obj, uint64_t code)
{
  Scheduler *sched = static_cast<Scheduler*>(obj);
  pthread_mutex_lock(&sched->lock);
  dbg_assert(code < UINT64_MAX);
  sched->code = (int)code;
  sched->state = SCHED_STOP;
  pthread_cond_broadcast(&sched->stopped);
  pthread_mutex_unlock(&sched->lock);
}

static int
_scheduler_stop_async_handler(void)
{
  scheduler_stop(here->sched, HPX_SUCCESS);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _scheduler_stop_async,
                     _scheduler_stop_async_handler);

int
scheduler_is_stopped(const void *obj)
{
  const Scheduler *sched = static_cast<const Scheduler*>(obj);
  return (sync_load(&sched->state, SYNC_ACQUIRE) != SCHED_RUN);
}

/// Exit a diffuse epoch.
///
/// This is called from the context of a lightweight thread, and needs to
/// broadcast the stop signal along with the output value.
///
/// This is deceptively complex when we have synchronous network progress (i.e.,
/// when the scheduler is responsible for calling network progress from the
/// schedule loop) because we can't stop the scheduler until we are sure that
/// the signal has made it out. We use the network_send operation manually here
/// because it allows us to wait for the `ssync` event (this event means that
/// we're guaranteed that we don't need to keep progressing locally for the send
/// to be seen remotely).
///
/// Don't perform the local shutdown until we're sure all the remote shutdowns
/// have gotten out, otherwise we might not progress the network enough.
static void
_scheduler_exit_diffuse(void *obj, size_t size, const void *out)
{
  Scheduler *sched = static_cast<Scheduler*>(obj);

  hpx_addr_t sync = hpx_lco_and_new(here->ranks - 1);
  for (auto i = 0u, e = here->ranks; i < e; ++i) {
    if (i == here->rank) continue;

    hpx_parcel_t *p = action_new_parcel(_scheduler_set_output_async, // action
                                        HPX_THERE(i),                // target
                                        HPX_THERE(i), // continuation target
                                        _scheduler_stop_async, // continuation action
                                        2,            // number of args
                                        out,          // arg 0
                                        size);        // the 1
    hpx_parcel_t *q = action_new_parcel(hpx_lco_set_action, // action
                                        sync,               // target
                                        0,    // continuation target
                                        0,    // continuation action
                                        0);   // number of args

    parcel_prepare(p);
    parcel_prepare(q);
    dbg_check( network_send(here->net, p, q) );
  }
  dbg_check( hpx_lco_wait(sync) );
  hpx_lco_delete_sync(sync);

  scheduler_set_output(sched, size, out);
  scheduler_stop(sched, HPX_SUCCESS);
}

static int
_scheduler_terminate_spmd_handler(void)
{
  static volatile unsigned _count = 0;
  if (sync_addf(&_count, 1, SYNC_RELAXED) != here->ranks) return HPX_SUCCESS;

  hpx_addr_t sync = hpx_lco_and_new(here->ranks - 1);
  for (auto i = 0u, e = here->ranks; i < e; ++i) {
    if (i == here->rank) continue;

    hpx_parcel_t *p = action_new_parcel(_scheduler_stop_async, // action
                                        HPX_THERE(i),          // target
                                        0,      // continuation target
                                        0,      // continuation action
                                        0);     // number of args
    hpx_parcel_t *q = action_new_parcel(hpx_lco_set_action, // action
                                        sync,               // target
                                        0,      // continuation target
                                        0,      // continuation action
                                        0);     // number of args

    parcel_prepare(p);
    parcel_prepare(q);
    dbg_check( network_send(here->net, p, q) );
  }
  dbg_check( hpx_lco_wait(sync) );
  hpx_lco_delete_sync(sync);
  scheduler_stop(here->sched, HPX_SUCCESS);
  sync_store(&_count, 0, SYNC_RELAXED);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _scheduler_terminate_spmd,
                     _scheduler_terminate_spmd_handler);

static void
_scheduler_exit_spmd(void *sched, size_t size, const void *out)
{
  scheduler_set_output(sched, size, out);
  hpx_call(HPX_THERE(0), _scheduler_terminate_spmd, HPX_NULL);
}

void
scheduler_exit(void *obj, size_t size, const void *out)
{
  Scheduler *sched = static_cast<Scheduler*>(obj);
  if (sched->spmd) {
    _scheduler_exit_spmd(sched, size, out);
  }
  else {
    _scheduler_exit_diffuse(sched, size, out);
  }
  hpx_thread_exit(HPX_SUCCESS);
}

void
scheduler_spawn(hpx_parcel_t *p)
{
  assert(self && "Spawn called on non-scheduler thread");
  self->spawn(p);
}

// Spawn a parcel on a specified worker thread.
void
scheduler_spawn_at(hpx_parcel_t *p, int thread)
{
  dbg_assert(p);
  dbg_assert(thread >= 0);
  dbg_assert(here && here->sched && (here->sched->n_workers > thread));
  here->sched->workers[thread]->pushMail(p);
}

hpx_parcel_t *
scheduler_current_parcel(void)
{
  dbg_assert(self);
  return self->getCurrentParcel();
}

int
scheduler_get_n_workers(const void *obj)
{
  auto *sched = static_cast<const Scheduler*>(obj);
  return sched->n_workers;
}

void
scheduler_signal(void *cond)
{
  Condition* condition = static_cast<Condition*>(cond);
  parcel_launch_all(condition->pop());
}

void
scheduler_signal_all(void *cond)
{
  Condition* condition = static_cast<Condition*>(cond);
  parcel_launch_all(condition->popAll());
}

void
scheduler_signal_error(void* cond, hpx_status_t code)
{
  Condition* condition = static_cast<Condition*>(cond);
  parcel_launch_all(condition->setError(code));
}

hpx_status_t
scheduler_wait(void *lco, void *cond)
{
  Condition* condition = static_cast<Condition*>(cond);
  // push the current thread onto the condition variable---no lost-update
  // problem here because we're holing the @p lock
  Worker *w = self;
  hpx_parcel_t *p = w->getCurrentParcel();

  // we had better be holding a lock here
  dbg_assert(p->thread->inLCO());

  if (hpx_status_t status = condition->push(p)) {
    return status;
  }

  w->EVENT_THREAD_SUSPEND(p);
  w->schedule([lco](hpx_parcel_t* p) {
      static_cast<LCO*>(lco)->unlock(p);
    });
  self->EVENT_THREAD_RESUME(p);

  // reacquire the lco lock before returning
  static_cast<LCO*>(lco)->lock(p);
  return condition->getError();
}

void
scheduler_yield(void)
{
  Worker *w = self;
  dbg_assert(action_is_default(w->getCurrentParcel()->action));
  EVENT_SCHED_YIELD();
  w->EVENT_THREAD_SUSPEND(w->getCurrentParcel());
  w->schedule([w](hpx_parcel_t* p) {
      dbg_assert(w == self);
      w->pushYield(p);
    });
  self->EVENT_THREAD_RESUME(w->getCurrentParcel());
}

void
scheduler_suspend(void (*f)(hpx_parcel_t *, void*), void *env)
{
  Worker *w = self;
  hpx_parcel_t *p = w->getCurrentParcel();
  log_sched("suspending %p in %s\n", p, actions[p->action].key);
  w->EVENT_THREAD_SUSPEND(p);
  w->schedule(std::bind(f, std::placeholders::_1, env));
  self->EVENT_THREAD_RESUME(p);
  log_sched("resuming %p in %s\n", p, actions[p->action].key);
}
