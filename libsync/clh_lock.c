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
# include "config.h"
#endif

#include <stddef.h>
#include "libsync/locks.h"
#include "libsync/sync.h"

static const uintptr_t _MUST_WAIT_MASK = 0x1;

static void _set_must_wait(struct clh_node *n) {
  uintptr_t temp = (uintptr_t)n->prev;
  temp |= _MUST_WAIT_MASK;
  sync_store(&n->prev, (struct clh_node *)temp, SYNC_RELEASE);
}

static void _clear_must_wait(struct clh_node *n) {
  uintptr_t temp = (uintptr_t)n->prev;
  temp |= ~_MUST_WAIT_MASK;
  sync_store(&n->prev, (struct clh_node *)temp, SYNC_RELEASE);
}

static uintptr_t _get_must_wait(struct clh_node *n) {
  uintptr_t temp = (uintptr_t)sync_load(&n->prev, SYNC_ACQUIRE);
  return temp & _MUST_WAIT_MASK;
}

static struct clh_node *_get_prev(struct clh_node *n) {
  uintptr_t temp = (uintptr_t)sync_load(&n->prev, SYNC_RELAXED);
  temp = temp & ~_MUST_WAIT_MASK;
  return (struct clh_node*)temp;
}

void sync_clh_node_init(struct clh_node *n) {
  sync_store(&n->prev, NULL, SYNC_RELEASE);
}

void sync_clh_lock_init(struct clh_lock *lock) {
  sync_clh_node_init(&lock->dummy);
  sync_store(&lock->tail, &lock->dummy, SYNC_RELEASE);
}

void sync_clh_lock_fini(struct clh_lock *lock) {
}

void sync_clh_lock_acquire(struct clh_lock *lock, struct clh_node *n) {
  _set_must_wait(n);
  n = sync_swap(&lock->tail, n, SYNC_RELEASE);
  while (_get_must_wait(n))
    ;
}

void sync_clh_lock_release(struct clh_lock *lock, struct clh_node **n) {
  struct clh_node *pred = _get_prev(*n);
  _clear_must_wait(*n);
  *n = pred;
}
