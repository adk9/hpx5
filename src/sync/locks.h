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
#ifndef LIBHPX_SYNC_LOCKS_H_
#define LIBHPX_SYNC_LOCKS_H_

#include <stdint.h>
#include "sync.h"


typedef struct {
    uintptr_t lock;
} tatas_t;

void hpx_sync_tatas_init(tatas_t *l);
void hpx_sync_tatas_acquire_slow(tatas_t*);
void hpx_sync_tatas_acquire(tatas_t* l) {
    if (hpx_sync_swap(&l->lock, 1, HPX_SYNC_ACQUIRE))
        hpx_sync_tatas_acquire_slow(l);
}
void hpx_sync_tatas_release(tatas_t* l) {
    hpx_sync_store(&l->lock, 0, HPX_SYNC_RELEASE);
}

#endif /* LIBHPX_SYNC_LOCKS_H_ */
