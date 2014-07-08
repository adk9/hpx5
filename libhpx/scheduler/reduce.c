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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// @file libhpx/scheduler/reduce.c
/// @brief Defines the reduction LCO.

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "cvar.h"
#include "lco.h"

/// Local reduce interface.
/// @{
static const int _reducing = 0;
static const  int _reading = 1;

typedef struct {
  lco_t                           lco;
  cvar_t                         wait;
  size_t                 participants;
  hpx_commutative_associative_op_t op;
  void                 (*init)(void*);
  size_t                        count;
  volatile int                  phase;
  void                         *value;     // out-of-place for alignment reasons
} _reduce_t;


/// This utility reduces the count of waiting
static int
_reduce_join(_reduce_t *r)
{
  assert(r->count != 0);

  if (--r->count > 0)
    return 0;

  r->phase = 1 - r->phase;
  r->count = r->participants;
  scheduler_signal_all(&r->wait);
  return 1;
}

/// Deletes a reduction.
static void
_reduce_fini(lco_t *lco)
{
  lco_lock(lco);
  _reduce_t *r = (_reduce_t *)lco;
  free(r->value);
  free(r);
}


/// Handle an error condition.
static void
_reduce_error(lco_t *lco, hpx_status_t code)
{
  lco_lock(lco);
  _reduce_t *r = (_reduce_t *)lco;
  scheduler_signal_error(&r->wait, code);
  lco_unlock(lco);
}


/// Update the reduction, will wait if the phase is reading.
static void
_reduce_set(lco_t *lco, int size, const void *from)
{
  lco_lock(lco);
  _reduce_t *r = (_reduce_t *)lco;

  // wait until we're reducing, then perform the op() and join the reduction
  while (r->phase != _reducing)
    scheduler_wait(&lco->lock, &r->wait);
  r->op(r->value, from);
  _reduce_join(r);

  lco_unlock(lco);
}


/// Get the value of the reduction, will wait if the phase is reducing.
static hpx_status_t
_reduce_get(lco_t *lco, int size, void *out)
{
  _reduce_t *r = (_reduce_t *)lco;
  lco_lock(lco);

  hpx_status_t status = cvar_get_error(&r->wait);

  // wait until we're reading, then read the value and join the reduction
  while ((r->phase != _reading) && (status == HPX_SUCCESS))
    status = scheduler_wait(&lco->lock, &r->wait);

  if (status == HPX_SUCCESS) {
    if (size && out)
      memcpy(out, r->value, size);
    if (_reduce_join(r))
      r->init(r->value);
  }

  lco_unlock(lco);
  return status;
}


// Wait for the reduction, loses the value of the reduction for this round.
static hpx_status_t
_reduce_wait(lco_t *lco)
{
  return _reduce_get(lco, 0, NULL);
}


static void
_reduce_init(_reduce_t *r, size_t participants, size_t size,
             hpx_commutative_associative_op_t op, void (*init)(void *))
{
  // vtable
  static const lco_class_t vtable = {
    _reduce_fini,
    _reduce_error,
    _reduce_set,
    _reduce_get,
    _reduce_wait
  };

  assert(init);

  lco_init(&r->lco, &vtable, 0);
  cvar_reset(&r->wait);
  r->participants = participants;
  r->op = op;
  r->init = init;
  r->count = participants;
  r->phase = _reducing;
  r->value = NULL;

  if (size) {
    r->value = malloc(size);
    assert(r->value);
  }

  r->init(r->value);
}

/// @}

/// Allocate a reduction LCO.
///
/// @param participants The static number of participants in the reduction.
/// @param op           The commutative associative reduction operation.
/// @param size         The size of the data we're reducing
hpx_addr_t
hpx_lco_reduce_new(size_t inputs, size_t size,
                   hpx_commutative_associative_op_t op, void (*init)(void*))
{
  hpx_addr_t reduce = locality_malloc(sizeof(_reduce_t));
  _reduce_t *r = NULL;
  if (!hpx_gas_try_pin(reduce, (void**)&r)) {
    dbg_error("Could not pin newly allocated reduction.\n");
    hpx_abort();
  }
  _reduce_init(r, inputs, size, op, init);
  hpx_gas_unpin(reduce);
  return reduce;
}
