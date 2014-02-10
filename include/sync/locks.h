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

#pragma once
#ifndef HPX_SYNC_LOCKS_H_
#define HPX_SYNC_LOCKS_H_

#include "sync.h"

struct tatas_lock {
    int lock;
};

typedef struct tatas_lock tatas_lock_t;

#define TATAS_INIT {0}

void tatas_init(tatas_lock_t *l);

void tatas_acquire_slow(tatas_lock_t *l);

static inline void tatas_acquire(tatas_lock_t *l) {
    if (sync_swap(&l->lock, 1, SYNC_ACQUIRE))
        tatas_acquire_slow(l);
}
static inline void tatas_release(tatas_lock_t *l) {
    sync_store(&l->lock, 0, SYNC_RELEASE);
}

#endif /* HPX_SYNC_LOCKS_H_ */
