// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// @file libhpx/scheduler/lco.c
/// ----------------------------------------------------------------------------
#include <stdint.h>
#include "sync/sync.h"
#include "libhpx/scheduler.h"
#include "lco.h"
#include "thread.h"


/// We pack state into the LCO. The least-significant bit is the LOCK bit, the
/// second least significant bit is the SET bit, and the third least significant
/// bit is the USER bit, which can be used by "subclasses" to store subclass
/// specific date, if desired.
#define _LOCK_MASK   (0x1)
#define _SET_MASK    (0x2)
#define _USER_MASK   (0x4)
#define _STATE_MASK  (0x7)
#define _QUEUE_MASK  ~_STATE_MASK
#define _UNLOCK_MASK ~_LOCK_MASK

void
lco_init(lco_t *lco, int user) {
  uintptr_t bits = (user) ? _USER_MASK : 0;
  lco->queue = (thread_t *)bits;
}

void
lco_set_user(lco_t *lco) {
  uintptr_t bits = (uintptr_t)lco->queue;
  bits |= _USER_MASK;
  lco->queue = (thread_t*)bits;
}


int
lco_is_user(const lco_t *lco) {
  uintptr_t bits = (uintptr_t)lco->queue;
  return bits & _USER_MASK;
}


int
lco_is_set(const lco_t *lco) {
  uintptr_t bits = (uintptr_t)lco->queue;
  return bits & _SET_MASK;
}


void
lco_lock(lco_t *lco) {
  while (true) {
    // load the queue pointer
    thread_t *from = sync_load(&lco->queue, SYNC_RELAXED);

    // if the lock bit is set, yield and then try again
    uintptr_t bits = (uintptr_t)from;
    if (bits & _LOCK_MASK) {
      scheduler_yield();
      continue;
    }

    // generate a to value for the queue that is locked, and try and cas it into
    // the queue
    thread_t *to = (thread_t *)(bits | _LOCK_MASK);
    if (!sync_cas(&lco->queue, from, to, SYNC_ACQUIRE, SYNC_RELAXED)) {
      scheduler_yield();
      continue;
    }

    // succeeded
    return;
  }
}


void
lco_unlock(lco_t *lco) {
  // we hold the lco, create an unlocked version of the queue, and write it out
  uintptr_t bits = (uintptr_t)lco->queue;
  thread_t *to = (thread_t *)(bits & _UNLOCK_MASK);
  sync_store(&lco->queue, to, SYNC_RELEASE);
}


thread_t *
lco_trigger(lco_t *lco) {
  uintptr_t bits = (uintptr_t)lco->queue;
  thread_t *queue = (thread_t*)(bits & _QUEUE_MASK);
  bits &= _STATE_MASK;
  bits |= _SET_MASK;
  thread_t *to = (thread_t*)bits;
  sync_store(&lco->queue, to, SYNC_RELAXED);
  return queue;
}


void
lco_enqueue_and_unlock(lco_t *lco, thread_t *thread) {
  uintptr_t bits = (uintptr_t)lco->queue;
  thread_t *queue = (thread_t*)(bits & _QUEUE_MASK);
  thread->next = queue;
  uintptr_t bits2 = (uintptr_t)thread;
  bits2 &= _UNLOCK_MASK;
  thread = (thread_t *)bits2;
  sync_store(&lco->queue, thread, SYNC_RELEASE);
}
