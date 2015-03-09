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

/// @file libhpx/scheduler/allreduce.c
/// @brief Defines the all-reduction LCO.

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
typedef struct {
  lco_t               lco;
  cvar_t             wait;
  size_t          readers;
  size_t          writers;
  hpx_monoid_id_t      id;
  hpx_monoid_op_t      op;
  size_t            count;
  volatile int      phase;
  void             *value;  // out-of-place for alignment reasons
} _allreduce_t;

static const int _reducing = 0;
static const  int _reading = 1;

static size_t _allreduce_size(lco_t *lco) {
  _allreduce_t *allreduce = (_allreduce_t *)lco;
  return sizeof(*allreduce);
}

/// Deletes a reduction.
static void _allreduce_fini(lco_t *lco) {
  lco_lock(lco);
  _allreduce_t *r = (_allreduce_t *)lco;
  if (r->value) {
    free(r->value);
  }
  lco_fini(lco);
  libhpx_global_free(lco);
}

/// Handle an error condition.
static void _allreduce_error(lco_t *lco, hpx_status_t code) {
  lco_lock(lco);
  _allreduce_t *r = (_allreduce_t *)lco;
  scheduler_signal_error(&r->wait, code);
  lco_unlock(lco);
}

static void _allreduce_reset(lco_t *lco) {
  _allreduce_t *r = (_allreduce_t *)lco;
  lco_lock(&r->lco);
  dbg_assert_str(cvar_empty(&r->wait),
                 "Reset on allreduce LCO that has waiting threads.\n");
  cvar_reset(&r->wait);
  lco_unlock(&r->lco);
}

/// Update the reduction, will wait if the phase is reading.
static void _allreduce_set(lco_t *lco, int size, const void *from) {
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
static hpx_status_t _allreduce_get(lco_t *lco, int size, void *out) {
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
    r->id(r->value, size);
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
static hpx_status_t _allreduce_wait(lco_t *lco) {
  return _allreduce_get(lco, 0, NULL);
}

static void _allreduce_init(_allreduce_t *r, size_t writers, size_t readers,
                            size_t size, hpx_monoid_id_t id, hpx_monoid_op_t op) {
  // vtable
  static const lco_class_t vtable = {
    .on_fini     = _allreduce_fini,
    .on_error    = _allreduce_error,
    .on_set      = _allreduce_set,
    .on_attach   = NULL,
    .on_get      = _allreduce_get,
    .on_getref   = NULL,
    .on_release  = NULL,
    .on_wait     = _allreduce_wait,
    .on_reset    = _allreduce_reset,
    .on_size     = _allreduce_size
  };

  assert(id);
  assert(op);

  lco_init(&r->lco, &vtable);
  cvar_reset(&r->wait);
  r->readers = readers;
  r->op = op;
  r->id = id;
  r->count = writers;
  r->writers = writers;
  r->phase = _reducing;
  r->value = NULL;

  if (size) {
    r->value = malloc(size);
    assert(r->value);
  }

  r->id(r->value, size);
}

/// @}

hpx_addr_t hpx_lco_allreduce_new(size_t inputs, size_t outputs, size_t size,
                                 hpx_monoid_id_t id, hpx_monoid_op_t op) {
  _allreduce_t *r = libhpx_global_malloc(sizeof(*r));
  assert(r);
  _allreduce_init(r, inputs, outputs, size, id, op);
  return lva_to_gva(r);
}


/// Initialize a block of array of lco.
static int _block_local_init_handler(int n, size_t participants, size_t readers,
                                     size_t size, hpx_monoid_id_t id,
                                     hpx_monoid_op_t op) {
  void *lco = hpx_thread_current_local_target();
  dbg_assert(lco);

  for (int i = 0; i < n; i++) {
    void *addr = (void *)((uintptr_t)lco + i * (sizeof(_allreduce_t) + size));
    _allreduce_init(addr, participants, readers, size, id, op);
  }
  return HPX_SUCCESS;
}

static HPX_ACTION_DEF(PINNED, _block_local_init_handler, _block_local_init,
                      HPX_INT, HPX_SIZE_T, HPX_SIZE_T, HPX_SIZE_T,
                      HPX_POINTER, HPX_POINTER);


/// Allocate an array of allreduce LCO local to the calling locality.
/// @param            n The (total) number of lcos to allocate
/// @param participants The static number of participants in the reduction.
/// @param      readers The static number of the readers of the result of the reduction.
/// @param         size The size of the data being reduced.
/// @param           id An initialization function for the data, this is
///                     used to initialize the data in every epoch.
/// @param           op The commutative-associative operation we're
///                     performing.
///
/// @returns the global address of the allocated array lco.
hpx_addr_t hpx_lco_allreduce_local_array_new(int n, size_t participants,
                                             size_t readers, size_t size,
                                             hpx_monoid_id_t id,
                                             hpx_monoid_op_t op) {
  uint32_t lco_bytes = sizeof(_allreduce_t) + size;
  dbg_assert(n * lco_bytes < UINT32_MAX);
  uint32_t  block_bytes = n * lco_bytes;
  hpx_addr_t base = hpx_gas_alloc(block_bytes);

  int e = hpx_call_sync(base, _block_local_init, NULL, 0, &n, &participants, &readers,
                        &size, &id, &op);
  dbg_check(e, "call of _block_init_action failed\n");

  // return the base address of the allocation
  return base;
}
