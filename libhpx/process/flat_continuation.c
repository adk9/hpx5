// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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

#include <stdlib.h>
#include <hpx/hpx.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/padding.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>

typedef struct {
  hpx_parcel_t *stack;
  PAD_TO_CACHELINE(sizeof(hpx_parcel_t *));
} _slot_t;

void *flat_continuation_new(void) {
  int n = here->sched->n_workers;
  int bytes = n * sizeof(_slot_t);
  int align = HPX_CACHELINE_SIZE;
  _slot_t *slots = NULL;
  posix_memalign((void*)&slots, align, bytes);
  dbg_assert(slots);
  memset(slots, 0, bytes);
  return slots;
}

void flat_continuation_delete(void *obj) {
  if (obj) {
    free(obj);
  }
}

void flat_continuation_wait(void *obj, hpx_parcel_t *p) {
  _slot_t *slots = obj;
  _slot_t *slot = &slots[self->id];
  parcel_stack_push(&slot->stack, p);
}

void flat_continuation_trigger(void *obj, const void *value, size_t bytes) {
  _slot_t *slots = obj;
  int n = here->sched->n_workers;

  // Start by extracting all of the continuation stacks---as soon as we start
  // sending these out, new ones may arrive. This will prevent issues across
  // multiple iterations.
  dbg_assert( hpx_thread_can_alloca(n * sizeof(hpx_parcel_t *)) );
  hpx_parcel_t *stacks[n];
  for (int i = 0, e = n; i < e; ++i) {
    stacks[i] = slots[i].stack;
    slots[i].stack = NULL;
  }

  // Send all of the waiting parcels. I spot-tested sending these back to the
  // thread that they waited from directly using the worker mailboxes, but that
  // has a slight adverse effect.
  hpx_parcel_t *p = NULL;
  for (int i = 0, e = n; i < e; ++i) {
    while ((p = parcel_stack_pop(&stacks[i]))) {
      hpx_parcel_set_data(p, value, bytes);
      parcel_launch(p);
    }
  }
}
