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
#include "events.h"
#include "thread.h"
#include "libhpx/debug.h"
#include "libhpx/events.h"
#include "libhpx/locality.h"
#include "libhpx/Scheduler.h"

static void*
Worker::operator new(size_t bytes, void *addr)
{
  dbg_assert(((uintptr_t)addr & (HPX_CACHELINE_SIZE - 1)) == 0);
  return addr;
}


Worker::Worker(Scheduler *sched, int id)
    : thread(0),
      id(id),
      seed(id),
      work_first(0),
      nstacks(0),
      yielded(0),
      active(1),
      last_victim(-1),
      numa_node(-1),
      profiler(nullptr),
      bst(nullptr),
      network(here->net),
      logs(nullptr),
      stats(nullptr),
      sched(sched),
      system(nullptr),
      current(nullptr),
      stacks(nullptr),
      _pad1(),
      lock(PTHREAD_MUTEX_INITIALIZER),
      running(PTHREAD_COND_INITIALIZER),
      state(),
      work_id(0),
      _pad2(),
      queues(),
      inbox()
{
  sync_store(&state, SCHED_STOP, SYNC_RELAXED);
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

  while (ustack_t* stack = stacks) {
    stacks = stack->next;
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
  INST_IF (p) {
    EVENT_SCHED_POP_LIFO(p->id);
    EVENT_SCHED_WQSIZE(queues[work_id].size());
  }
  return p;
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
