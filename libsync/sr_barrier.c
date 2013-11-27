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

#include "sync.h"
#include "barriers.h"

struct sr_barrier {
  int count;
  int threads;
  int sense;
  int senses[];
};

sr_barrier_t *sr_barrier_new(int n) {
  sr_barrier_t *barrier = malloc(sizeof(sr_barrier_t) + n * sizeof(int));
  assert(barrier && "Could not allocate a sense-reversing barrier.");
  barrier->count = 0;
  barrier->threads = n;
  barrier->sense = 1;
  for (int i = 0; i < n; ++i)
    barrier->senses[i] = 1;
  return barrier;
}

void sr_barrier_delete(sr_barrier_t *barrier) {
  free(barrier);
}

void sr_barrier_join(sr_barrier_t *barrier, int tid) {
  int sense = 1 - barrier->senses[tid];
  barrier->senses[tid] = sense;

  if (sync_fadd(&barrier->count, 1, SYNC_ACQ_REL) == (barrier->threads - 1)) {
    sync_store(&barrier->count, 0, SYNC_RELAXED);
    sync_store(&barrier->sense, sense, SYNC_RELEASE);
  }
  else {
    while (sync_load(&barrier->sense, SYNC_ACQUIRE) != sense)
      /* nop */;
  }
  sync_fence(SYNC_RELEASE);
}

