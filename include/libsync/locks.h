/*
  ====================================================================
  High Performance ParalleX Library (libhpx)

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/
#ifndef HPX_SYNC_LOCKS_H_
#define HPX_SYNC_LOCKS_H_

#include "sync.h"
#include "attributes.h"

struct tatas_lock {
    volatile int lock;
};

typedef struct tatas_lock tatas_lock_t;

#define TATAS_INIT {0}

HPX_INTERNAL void tatas_init(tatas_lock_t *l) HPX_NON_NULL(1);
HPX_INTERNAL void tatas_acquire_slow(tatas_lock_t *l) HPX_NON_NULL(1);
static inline void tatas_acquire(tatas_lock_t *l) HPX_NON_NULL(1);
static inline void tatas_release(tatas_lock_t *l) HPX_NON_NULL(1);

void
tatas_acquire(tatas_lock_t *l) {
  if (sync_swap(&l->lock, 1, SYNC_ACQUIRE))
    tatas_acquire_slow(l);
}

void
tatas_release(tatas_lock_t *l) {
  sync_store(&l->lock, 0, SYNC_RELEASE);
}

#endif /* HPX_SYNC_LOCKS_H_ */
