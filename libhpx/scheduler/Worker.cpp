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
#include "libhpx/debug.h"
#include "libhpx/events.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/memory.h"
#include "libhpx/Scheduler.h"
#include "libhpx/topology.h"
#include <cstring>
#ifdef HAVE_URCU
# include <urcu-qsbr.h>
#endif

namespace {
using libhpx::Network;
}

Worker::Worker(Scheduler& sched, Network& network, int id)
    : thread(0),
      id(id),
      seed(id),
      work_first(0),
      nstacks_(0),
      yielded(0),
      active(1),
      last_victim(-1),
      numa_node(-1),
      profiler_(nullptr),
      bst(nullptr),
      network_(network),
      logs(nullptr),
      stats(nullptr),
      sched_(sched),
      system_(nullptr),
      current(nullptr),
      stacks_(nullptr),
      _pad1(),
      lock(PTHREAD_MUTEX_INITIALIZER),
      running(PTHREAD_COND_INITIALIZER),
      state(SCHED_STOP),
      work_id(0),
      _pad2(),
      queues(),
      inbox()
{
  dbg_assert(((uintptr_t)this & (HPX_CACHELINE_SIZE - 1)) == 0);
  sync_two_lock_queue_init(&inbox, NULL);
}

Worker::~Worker() {
  if (pthread_cond_destroy(&running)) {
    dbg_error("Failed to destroy the running condition for %d\n", id);
  }

  if (pthread_mutex_destroy(&lock)) {
    dbg_error("Failed to destroy the lock for %d\n", id);
  }

  if (hpx_parcel_t* p = handleMail()) {
    parcel_delete(p);
  }
  sync_two_lock_queue_fini(&inbox);

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
  uint64_t size = queues[work_id].push(p);
  if (work_first >= 0) {
    work_first = (here->config->sched_wfthreshold < size);
  }
}

hpx_parcel_t*
Worker::popLIFO()
{
  hpx_parcel_t *p = queues[work_id].pop();
  dbg_assert(!p || p != current);
  INST_IF (p) {
    EVENT_SCHED_POP_LIFO(p->id);
    EVENT_SCHED_WQSIZE(queues[work_id].size());
  }
  return p;
}

hpx_parcel_t *
Worker::handleNetwork()
{
  // don't do work first scheduling in the network
  int wf = work_first;
  work_first = -1;
  network_.progress(0);

  hpx_parcel_t *stack = network_.probe(0);
  work_first = wf;

  while (hpx_parcel_t *p = parcel_stack_pop(&stack)) {
    pushLIFO(p);
  }

  return popLIFO();
}

hpx_parcel_t*
Worker::handleMail()
{
  void *buffer = sync_two_lock_queue_dequeue(&inbox);
  hpx_parcel_t *parcels = static_cast<hpx_parcel_t*>(buffer);
  if (!parcels) {
    return NULL;
  }

  hpx_parcel_t *prev = parcel_stack_pop(&parcels);
  do {
    hpx_parcel_t *next = NULL;
    while ((next = parcel_stack_pop(&parcels))) {
      dbg_assert(next != current);
      EVENT_SCHED_MAIL(prev->id);
      log_sched("got mail %p\n", prev);
      pushLIFO(prev);
      prev = next;
    }
    buffer = sync_two_lock_queue_dequeue(&inbox);
    parcels = static_cast<hpx_parcel_t*>(buffer);
  } while (parcels);
  dbg_assert(prev);
  return prev;
}

bool
Worker::create()
{
  if (int e = pthread_create(&thread, NULL, Worker::Enter, this)) {
    dbg_error("cannot start worker thread %d (%s).\n", id, strerror(e));
    return false;
  }
  else {
    return true;
  }
}

void
Worker::join()
{
  if (thread == 0) {
    log_dflt("worker_join called on thread (%d) that didn't start\n", id);
    return;
  }

  int e = pthread_join(thread, NULL);
  if (e) {
    log_error("cannot join worker thread %d (%s).\n", id, strerror(e));
  }
}

void
Worker::stop()
{
  pthread_mutex_lock(&lock);
  state = SCHED_STOP;
  pthread_mutex_unlock(&lock);
}

void
Worker::start()
{
  pthread_mutex_lock(&lock);
  state = SCHED_RUN;
  pthread_cond_broadcast(&running);
  pthread_mutex_unlock(&lock);
}

void
Worker::shutdown()
{
  pthread_mutex_lock(&lock);
  state = SCHED_SHUTDOWN;
  pthread_cond_broadcast(&running);
  pthread_mutex_unlock(&lock);
}

void
Worker::bind(hpx_parcel_t *p)
{
  // try and get a stack from the freelist, otherwise allocate a new one
  ustack_t *stack = stacks_;
  if (stack) {
    stacks_ = stack->next;
    --nstacks_;
    thread_init(stack, p, Execute, stack->size);
  }
  else {
    stack = thread_new(p, Execute);
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
Worker::schedule(std::function<void(hpx_parcel_t*)>&& f)
{
  EVENT_SCHED_BEGIN();
  if (state != SCHED_RUN) {
    transfer(system_, std::forward<std::function<void(hpx_parcel_t*)>>(f));
  }
  else if (hpx_parcel_t *p = handleMail()) {
    transfer(p, std::forward<std::function<void(hpx_parcel_t*)>>(f));
  }
  else if (hpx_parcel_t *p = popLIFO()) {
    transfer(p, std::forward<std::function<void(hpx_parcel_t*)>>(f));
  }
  else {
    transfer(system_, std::forward<std::function<void(hpx_parcel_t*)>>(f));
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
  int status = system_set_worker_affinity(id, policy);
  if (status != LIBHPX_OK) {
    log_dflt("WARNING: running with no worker thread affinity. "
             "This MAY result in diminished performance.\n");
  }

  int cpu = id % here->topology->ncpus;
  numa_node = here->topology->cpu_to_numa[cpu];

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
  current = &system;
  work_first = 0;

  // Hang out here until we're shut down.
  while (state != SCHED_SHUTDOWN) {
    run();                                   // returns when state != SCHED_RUN
    sleep();                                 // returns when state != SCHED_STOP
  }

  system_ = NULL;
  current = NULL;

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
  pthread_mutex_lock(&lock);
  while (state == SCHED_STOP) {
    while (hpx_parcel_t *p = queues[1 - work_id].pop()) {
      pushLIFO(p);
    }

    if (hpx_parcel_t *p = handleMail()) {
      pushLIFO(p);
    }

    // go back to sleep
    sync_addf(&sched_.n_active, -1, SYNC_ACQ_REL);
    pthread_cond_wait(&running, &lock);
    sync_addf(&sched_.n_active, 1, SYNC_ACQ_REL);
  }
  pthread_mutex_unlock(&lock);
}

