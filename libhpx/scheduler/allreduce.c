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
#include <stdio.h>

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
  size_t                      readers;
  size_t                      writers;
  hpx_commutative_associative_op_t op;
  void   (*init)(void*, const size_t);
  size_t                        count;
  volatile int                  phase;
  void                         *value;     // out-of-place for alignment reasons
} _allreduce_t;

/// Deletes a reduction.
static void
_allreduce_fini(lco_t *lco)
{
  lco_lock(lco);
  _allreduce_t *r = (_allreduce_t *)lco;
  if (r->value)
    free(r->value);
}


/// Handle an error condition.
static void
_allreduce_error(lco_t *lco, hpx_status_t code)
{
  lco_lock(lco);
  _allreduce_t *r = (_allreduce_t *)lco;
  scheduler_signal_error(&r->wait, code);
  lco_unlock(lco);
}


/// Update the reduction, will wait if the phase is reading.
static void
_allreduce_set(lco_t *lco, int size, const void *from)
{
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _allreduce_t *r = (_allreduce_t *)lco;

  // wait until we're reducing, then perform the op() and join the reduction
  while ((r->phase != _reducing) && (status == HPX_SUCCESS)) {
    status = scheduler_wait(&lco->lock, &r->wait);
  }

  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  //perform the op()
  assert(size && from);
  r->op(r->value, from, size);

  // if we're the last one to arrive, switch the phase and signal readers.
  if (--r->count == 0) {
    r->phase = _reading;
    scheduler_signal_all(&r->wait);
  }

  unlock:
   lco_unlock(lco);
}


/// Get the value of the reduction, will wait if the phase is reducing.
static hpx_status_t
_allreduce_get(lco_t *lco, int size, void *out)
{
  _allreduce_t *r = (_allreduce_t *)lco;
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);

  // wait until we're reading
  while ((r->phase != _reading) && (status == HPX_SUCCESS)) {
    status = scheduler_wait(&lco->lock, &r->wait);
  }

  // if there was an error signal, unlock and return it
  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // copy out the value if the caller wants it
  if (size && out)
    memcpy(out, r->value, size);

  // update the count, if I'm the last reader to arrive, switch the mode and
  // release all of the other readers, otherwise wait for the phase to change
  // back to reducing---this blocking behavior prevents gets from one "epoch"
  // to satisfy earlier _reading epochs
  if (++r->count == r->readers) {
    r->count = r->writers;
    r->phase = _reducing;
    r->init(r->value, size);
    scheduler_signal_all(&r->wait);
  }
  else {
    while ((r->phase == _reading) && (status == HPX_SUCCESS))
      status = scheduler_wait(&r->lco.lock, &r->wait);
  }

  unlock:
   lco_unlock(lco);
   return status;
}


// Wait for the reduction, loses the value of the reduction for this round.
static hpx_status_t
_allreduce_wait(lco_t *lco)
{
  return _allreduce_get(lco, 0, NULL);
}


static void
_allreduce_init(_allreduce_t *r, size_t writers, size_t readers, size_t size,
                hpx_commutative_associative_op_t op,
                void (*init)(void *, const size_t size))
{
  // vtable
  static const lco_class_t vtable = {
    _allreduce_fini,
    _allreduce_error,
    _allreduce_set,
    _allreduce_get,
    _allreduce_wait
  };

  assert(init);

  lco_init(&r->lco, &vtable, 0);
  cvar_reset(&r->wait);
  r->readers = readers;
  r->op = op;
  r->init = init;
  r->count = writers;
  r->writers = writers;
  r->phase = _reducing;
  r->value = NULL;

  if (size) {
    r->value = malloc(size);
    assert(r->value);
  }

  r->init(r->value, size);
}

/// @}

hpx_addr_t
hpx_lco_allreduce_new(size_t inputs, size_t outputs, size_t size,
                      hpx_commutative_associative_op_t op,
                      void (*init)(void*, const size_t size))
{
  hpx_addr_t reduce = locality_malloc(sizeof(_allreduce_t));
  _allreduce_t *r = NULL;
  if (!hpx_gas_try_pin(reduce, (void**)&r)) {
    dbg_error("allreduce: could not pin newly allocated reduction.\n");
  }
  _allreduce_init(r, inputs, outputs, size, op, init);
  hpx_gas_unpin(reduce);
  return reduce;
}
