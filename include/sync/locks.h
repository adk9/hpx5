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

#include <stdint.h>
#include "sync.h"


struct tatas_lock {
    uintptr_t lock;
};

void tatas_init(struct tatas_lock *l);

void tatas_acquire_slow(struct tatas_lock *l);

void tatas_acquire(struct tatas_lock *l) {
    if (sync_swap(&l->lock, 1, SYNC_ACQUIRE))
        tatas_acquire_slow(l);
}
void tatas_release(struct tatas_lock *l) {
    sync_store(&l->lock, 0, SYNC_RELEASE);
}

#endif /* HPX_SYNC_LOCKS_H_ */
