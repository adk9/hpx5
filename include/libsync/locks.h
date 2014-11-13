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
#ifndef HPX_SYNC_LOCKS_H_
#define HPX_SYNC_LOCKS_H_

#include "sync.h"
#include "hpx/attributes.h"

struct tatas_lock {
    volatile int lock;
};

typedef struct tatas_lock tatas_lock_t;

#define SYNC_TATAS_LOCK_INIT {0}

HPX_INTERNAL void sync_tatas_init(tatas_lock_t *l) HPX_NON_NULL(1);
HPX_INTERNAL void sync_tatas_acquire_slow(tatas_lock_t *l) HPX_NON_NULL(1);
static inline void sync_tatas_acquire(tatas_lock_t *l) HPX_NON_NULL(1);
static inline void sync_tatas_release(tatas_lock_t *l) HPX_NON_NULL(1);

void
sync_tatas_acquire(tatas_lock_t *l) {
  if (sync_swap(&l->lock, 1, SYNC_ACQUIRE))
    sync_tatas_acquire_slow(l);
}

void
sync_tatas_release(tatas_lock_t *l) {
  sync_store(&l->lock, 0, SYNC_RELEASE);
}

struct clh_node {
  struct clh_node *prev;
  const char _padding[HPX_CACHELINE_SIZE - sizeof(struct clh_node*)];
} HPX_ALIGNED(HPX_CACHELINE_SIZE);

#define SYNC_CLH_NODE_INIT {                    \
    .prev = NULL                                \
    ._padding = {0}                             \
  }

void sync_clh_node_init(struct clh_node *n)
  HPX_NON_NULL(1) HPX_INTERNAL;

struct clh_lock {
  struct clh_node dummy;
  struct clh_node *tail;
} HPX_ALIGNED(HPX_CACHELINE_SIZE);

#define SYNC_CLH_LOCK_INIT {                    \
    .dummy = SYNC_CLH_NODE_INIT,                \
    .tail = NULL                                \
    }

void sync_clh_lock_init(struct clh_lock *lock)
  HPX_NON_NULL(1) HPX_INTERNAL;

void sync_clh_lock_fini(struct clh_lock *lock)
  HPX_NON_NULL(1) HPX_INTERNAL;

void sync_clh_lock_acquire(struct clh_lock *lock, struct clh_node *n)
  HPX_NON_NULL(1, 2) HPX_INTERNAL;

void sync_clh_lock_release(struct clh_lock *lock, struct clh_node **n)
  HPX_NON_NULL(1, 2) HPX_INTERNAL;

#endif /* HPX_SYNC_LOCKS_H_ */
