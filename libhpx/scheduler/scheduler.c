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

#include "libhpx/scheduler.h"
#include "thread.h"
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/process.h>
#include <libhpx/rebalancer.h>
#include <hpx/builtins.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

scheduler_t *
scheduler_new(const config_t *cfg)
{
  thread_set_stack_size(cfg->stacksize);

  const int workers = cfg->threads;
  scheduler_t *sched = NULL;
  size_t bytes = sizeof(*sched) + workers * sizeof(worker_t);
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

  // Initialize the worker data structures.
  for (int i = 0, e = workers; i < e; ++i) {
    worker_init(&sched->workers[i], sched, i);
  }

  // Start the worker threads.
  for (int i = 0, e = workers; i < e; ++i) {
    if (worker_create(&sched->workers[i])) {
      log_error("failed to initialize a worker during scheduler_new.\n");
      scheduler_delete(sched);
      return NULL;
    }
  }

  log_sched("initialized a new scheduler.\n");
  return sched;
}

void
scheduler_delete(scheduler_t *sched)
{
  // shutdown and join all of the worker threads
  for (int i = 0, e = sched->n_workers; i < e; ++i) {
    worker_shutdown(&sched->workers[i]);
    worker_join(&sched->workers[i]);
  }

  // clean up all of the worker data structures
  for (int i = 0, e = sched->n_workers; i < e; ++i) {
    worker_fini(&sched->workers[i]);
  }

  free(sched);
  as_leave();
}

worker_t *
scheduler_get_worker(scheduler_t *sched, int id)
{
  assert(id >= 0);
  assert(id < sched->n_workers);
  worker_t *w = &sched->workers[id];
  assert(((uintptr_t)w & (HPX_CACHELINE_SIZE - 1)) == 0);
  return w;
}

static void
_wait(void * const scheduler)
{
  scheduler_t * const csched = scheduler;
#ifdef HAVE_APEX
  int prev = csched->n_target;
  int n = min_int(apex_get_thread_cap(), csched->n_workers);
  log_sched("apex adjusting from %d to %d workers\n", prev, n);
  csched->n_target = n;
  void (*op)(worker_t*) = (n < prev) ? worker_stop : worker_start;
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
scheduler_start(scheduler_t *sched, int spmd, hpx_action_t act, void *out, int n,
                va_list *args)
{
  log_dflt("hpx started running %d\n", sched->epoch);

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
    worker_start(&sched->workers[i]);
  }

  // wait for someone to stop the scheduler
  pthread_mutex_lock(&sched->lock);
  while (sched->state == SCHED_RUN) {
    _wait(sched);
  }
  pthread_mutex_unlock(&sched->lock);

  // stop all of the worker threads
  for (int i = 0, e = sched->n_target; i < e; ++i) {
    worker_stop(&sched->workers[i]);
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
scheduler_set_output(scheduler_t *sched, size_t bytes, const void *out)
{
  if (!bytes) return;
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
scheduler_stop(scheduler_t *sched, uint64_t code)
{
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
scheduler_is_stopped(const scheduler_t *sched)
{
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
_scheduler_exit_diffuse(scheduler_t *sched, size_t size, const void *out)
{
  hpx_addr_t sync = hpx_lco_and_new(here->ranks - 1);
  for (int i = 0, e = here->ranks; i < e; ++i) {
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
  static volatile int _count = 0;
  if (sync_addf(&_count, 1, SYNC_RELAXED) != here->ranks) return HPX_SUCCESS;

  hpx_addr_t sync = hpx_lco_and_new(here->ranks - 1);
  for (int i = 0, e = here->ranks; i < e; ++i) {
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
_scheduler_exit_spmd(scheduler_t *sched, size_t size, const void *out)
{
  scheduler_set_output(sched, size, out);
  hpx_call(HPX_THERE(0), _scheduler_terminate_spmd, HPX_NULL);
}

void
scheduler_exit(scheduler_t *sched, size_t size, const void *out)
{
  if (sched->spmd) {
    _scheduler_exit_spmd(sched, size, out);
  }
  else {
    _scheduler_exit_diffuse(sched, size, out);
  }
  hpx_thread_exit(HPX_SUCCESS);
}
