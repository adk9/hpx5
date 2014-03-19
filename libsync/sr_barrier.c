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
#include <assert.h>
#include <stdlib.h>

#include "libsync/barriers.h"
#include "libsync/sync.h"
#include "nop.h"

/// The sense-reversing barrier.
///
/// Could use some padding here, if we ever wanted this to be useful for
/// common-case code. On the other hand, the sense-reversing barrier is never
/// going to be super-scalable, so we're not worried about it.
typedef struct {
  barrier_t vtable;
  SYNC_ATOMIC(int count);
  int threads;
  SYNC_ATOMIC(int sense);
  int senses[];
} sr_barrier_t;

/// Delete member function.
static void _delete(barrier_t *barrier) {
  free(barrier);
}


/// Sense-reversing join member function.
///
/// see: http://www.morganclaypool.com/doi/abs/10.2200/S00499ED1V01Y201304CAC023
static int _join(barrier_t *barrier, int i) {
  sr_barrier_t *this = (sr_barrier_t*)barrier;
  int sense = 1 - this->senses[i];
  this->senses[i] = sense;

  // If I'm the last joiner, release everyone and return 1
  if (sync_fadd(&this->count, 1, SYNC_ACQ_REL) == (this->threads - 1)) {
    sync_store(&this->count, 0, SYNC_RELAXED);
    sync_store(&this->sense, sense, SYNC_RELEASE);
    return 1;
  }

  // Otherwise wait.
  while (sync_load(&this->sense, SYNC_ACQUIRE) != sense)
    sync_nop_mwait(&this->sense);
  sync_fence(SYNC_RELEASE);
  return 0;
}


barrier_t *sr_barrier_new(int n) {
  sr_barrier_t *barrier = malloc(sizeof(sr_barrier_t) + n * sizeof(int));
  assert(barrier && "Could not allocate a sense-reversing barrier.");
  barrier->vtable.delete = _delete;
  barrier->vtable.join   = _join;
  barrier->count         = 0;
  barrier->threads       = n;
  barrier->sense         = 1;

  for (int i = 0; i < n; ++i)
    barrier->senses[i]   = 1;

  return &barrier->vtable;
}


