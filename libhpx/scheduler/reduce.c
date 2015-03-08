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
#include "config.h"
#endif

/// @file libhpx/scheduler/reduce.c
/// @brief Defines the reduction LCO.

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/scheduler.h>
#include "cvar.h"
#include "lco.h"

/// Local reduce interface.
/// @{
typedef struct {
  lco_t              lco;
  cvar_t         barrier;
  hpx_monoid_id_t     id;
  hpx_monoid_op_t     op;
  size_t            size;
  int             inputs;
  volatile int remaining;
  void            *value;
} _reduce_t;

/// Deletes a reduction.
static void _reduce_fini(lco_t *lco) {
  if (!lco) {
    return;
  }

  lco_lock(lco);
  _reduce_t *r = (_reduce_t *)lco;
  if (r->value) {
    free(r->value);
  }
  lco_fini(lco);
  global_free(lco);
}

static hpx_status_t _reduce_attach(lco_t *lco, hpx_parcel_t *p) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _reduce_t *r = (_reduce_t *)lco;

  // if the reduce is still waiting
  if (r->remaining) {
    status = cvar_attach(&r->barrier, p);
    goto unlock;
  }

  // if the reduce has an error, then return that error without sending the
  // parcel
  //
  // NB: should we actually send some sort of error condition?
  status = cvar_get_error(&r->barrier);
  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // go ahead and send this parcel eagerly
  hpx_parcel_send(p, HPX_NULL);

 unlock:
  lco_unlock(lco);
  return status;
}

/// Handle an error condition.
static void _reduce_error(lco_t *lco, hpx_status_t code) {
  lco_lock(lco);
  _reduce_t *r = (_reduce_t *)lco;
  scheduler_signal_error(&r->barrier, code);
  lco_unlock(lco);
}

static void _reduce_reset(lco_t *lco) {
  _reduce_t *r = (_reduce_t *)lco;
  lco_lock(lco);
  dbg_assert_str(cvar_empty(&r->barrier),
                 "Reset on allreduce LCO that has waiting threads.\n");
  cvar_reset(&r->barrier);
  r->remaining = r->inputs;
  r->id(r->value, r->size);
  lco_unlock(lco);
}

/// Update the reduction.
static void _reduce_set(lco_t *lco, int size, const void *from) {
  lco_lock(lco);
  _reduce_t *r = (_reduce_t *)lco;

  // perform the op()
  assert(size && from);
  r->op(r->value, from, size);

  if (--r->remaining == 0) {
    scheduler_signal_all(&r->barrier);
  }
  else {
    log_lco("reduce: received input %d\n", r->remaining);
    dbg_assert_str(r->remaining > 0, "reduction: too many threads joined (%d).\n", r->remaining);
  }

  lco_unlock(lco);
}

/// Get the value of the reduction.
static hpx_status_t _reduce_get(lco_t *lco, int size, void *out) {
  _reduce_t *r = (_reduce_t *)lco;
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);

  int remaining = r->remaining;
  while (remaining > 0 && status == HPX_SUCCESS) {
    status = scheduler_wait(&lco->lock, &r->barrier);
    remaining = r->remaining;
  }

  // if there was an error signal, unlock and return it
  if (status != HPX_SUCCESS) {
    lco_unlock(lco);
    return status;
  }

  // copy out the value if the caller wants it
  if (size && out) {
    memcpy(out, r->value, size);
  }

  lco_unlock(lco);
  return status;
}

// Wait for the reduction.
static hpx_status_t _reduce_wait(lco_t *lco) {
  return _reduce_get(lco, 0, NULL);
}

  // vtable
static const lco_class_t _reduce_vtable = {
  .on_fini     = _reduce_fini,
  .on_error    = _reduce_error,
  .on_set      = _reduce_set,
  .on_attach   = _reduce_attach,
  .on_get      = _reduce_get,
  .on_getref   = NULL,
  .on_release  = NULL,
  .on_wait     = _reduce_wait,
  .on_reset    = _reduce_reset
};

static void _reduce_init(_reduce_t *r, int inputs, size_t size, hpx_monoid_id_t id,
                         hpx_monoid_op_t op) {
  assert(id);
  assert(op);

  lco_init(&r->lco, &_reduce_vtable);
  cvar_reset(&r->barrier);
  r->op = op;
  r->id = id;
  r->size = size;
  r->inputs = inputs;
  r->remaining = inputs;
  r->value = NULL;

  if (size) {
    r->value = malloc(size);
    assert(r->value);
  }

  r->id(r->value, size);
}
/// @}

hpx_addr_t hpx_lco_reduce_new(int inputs, size_t size, hpx_monoid_id_t id,
                              hpx_monoid_op_t op) {
  _reduce_t *r = global_malloc(sizeof(*r));
  assert(r);
  _reduce_init(r, inputs, size, id, op);
  return lva_to_gva(r);
}
