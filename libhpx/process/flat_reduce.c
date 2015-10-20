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
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/padding.h>
#include <libhpx/scheduler.h>
#include "flat_reduce.h"

typedef struct {
  volatile int i;
  int n;
  size_t bytes;
  size_t padded;
  hpx_monoid_id_t id;
  hpx_monoid_op_t op;
  PAD_TO_CACHELINE(2 * sizeof(int) +
                   2 * sizeof(size_t) +
                   2 * sizeof(void(*)()));
  char values[];
} _flat_reduce_t;

size_t flat_reduce_size(size_t bytes) {
  size_t padded = _BYTES(HPX_CACHELINE_SIZE, bytes);
  return sizeof(_flat_reduce_t) + here->sched->n_workers * padded;
}

int flat_reduce_join(void *obj, const void *value, void *out) {
  _flat_reduce_t *r = obj;

  // reduce into this worker's temporary slot
  void *buffer = r->values + self->id * r->padded;
  r->op(buffer, value, r->bytes);
  if (sync_fadd(&r->i, -1, SYNC_ACQ_REL) > 1) {
    return 0;
  }

  // reduce all of the slots into the output buffer, clearing them along the way
  r->id(out, r->bytes);
  for (int i = 0, e = here->sched->n_workers; i < e; ++i) {
    void *buffer = r->values + i * r->padded;
    r->op(out, buffer, r->bytes);
    r->id(buffer, r->bytes);
  }

  // reset the reduction counter
  sync_store(&r->i, r->n, SYNC_RELEASE);
  return 1;
}

static int _flat_reduce_init_handler(void *obj, int n, size_t bytes,
                                     hpx_action_t id, hpx_action_t op) {
  dbg_assert(((uintptr_t)obj & (HPX_CACHELINE_SIZE - 1)) == 0);

  _flat_reduce_t *r = obj;

  r->n = n;
  r->bytes = bytes;
  r->padded = _BYTES(HPX_CACHELINE_SIZE, bytes);
  r->id = (hpx_monoid_id_t)hpx_action_get_handler(id);
  r->op = (hpx_monoid_op_t)hpx_action_get_handler(op);
  sync_store(&r->i, n, SYNC_RELEASE);
  for (int i = 0, e = here->sched->n_workers; i < e; ++i) {
    void *value = r->values + i * r->padded;
    r->id(value, bytes);
  }

  return HPX_SUCCESS;
}
LIBHPX_ACTION(HPX_INTERRUPT, HPX_PINNED, flat_reduce_init,
              _flat_reduce_init_handler, HPX_POINTER, HPX_INT,
              HPX_SIZE_T, HPX_ACTION_T, HPX_ACTION_T);

static int _flat_reduce_fini_handler(void) {
  return HPX_SUCCESS;
}
LIBHPX_ACTION(HPX_INTERRUPT, 0, flat_reduce_fini, _flat_reduce_fini_handler);
