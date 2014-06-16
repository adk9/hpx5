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

/// ----------------------------------------------------------------------------
/// @file libhpx/scheduler/and.c
/// Defines the AND LCO.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdint.h>

#include "hpx/hpx.h"
#include "libsync/sync.h"
#include "libhpx/debug.h"
#include "libhpx/scheduler.h"
#include "lco.h"
#include "cvar.h"

/// ----------------------------------------------------------------------------
/// And LCO class interface.
/// ----------------------------------------------------------------------------
/// @{
typedef struct {
  lco_t   vtable;
  cvar_t barrier;
  SYNC_ATOMIC(int64_t value);                   // the threshold
} _and_t;


// Freelist
static __thread _and_t *_free_ands = NULL;


static void
_lock(_and_t *and)
{
  sync_lockable_ptr_lock((lockable_ptr_t*)&and->vtable);
}


static void
_unlock(_and_t *and)
{
  sync_lockable_ptr_unlock((lockable_ptr_t*)&and->vtable);
}

static hpx_status_t
_wait(_and_t *and)
{
  // Still use sync to load the value even though we're holding the LCO lock
  // because it is atomically decremented without holding the lock and we don't
  // want to induce a race on it.
  //
  // NB: we're holding the lock here, so even if a concurrent set reduces the
  //     count to zero, we will still be woken up
  int64_t val;
  sync_load(val, &and->value, SYNC_ACQUIRE);
  if (val)
    return scheduler_wait((lockable_ptr_t*)and, &and->barrier);
  else
    return and->barrier.status;
}


static void
_free(_and_t *and) {
  // repurpose vtable as a freelist "next" pointer
  and->vtable = (lco_class_t*)_free_ands;
  _free_ands = and;
}


static void
_and_fini(lco_t *lco, hpx_addr_t sync)
{
  if (!lco)
    return;

  _and_t *and = (_and_t *)lco;
  _lock(and);
  _free(and);

  if (!hpx_addr_eq(sync, HPX_NULL))
    hpx_lco_set(sync, NULL, 0, HPX_NULL);
}


static void
_and_error(lco_t *lco, hpx_status_t code, hpx_addr_t sync)
{
  _and_t *and = (_and_t *)lco;
  _lock(and);
  scheduler_signal(&and->barrier, code);
  _unlock(and);

  if (!hpx_addr_eq(sync, HPX_NULL))
    hpx_lco_set(sync, NULL, 0, HPX_NULL);
}

/// Fast set uses atomic ops to decrement the value, and signals when it gets to 0.
static void
_and_set(lco_t *lco, int size, const void *from, hpx_addr_t sync)
{
  _and_t *and = (_and_t *)lco;
  int64_t val = sync_fadd(&and->value, -1, SYNC_ACQ_REL);

  if (val < 1) {
    dbg_error("Too many threads joined AND lco\n");
    return;
  }

  if (val > 1)
    return;

  _lock(and);
  scheduler_signal(&and->barrier, HPX_SUCCESS);
  _unlock(and);

  if (!hpx_addr_eq(sync, HPX_NULL))
    hpx_lco_set(sync, NULL, 0, HPX_NULL);
}


static hpx_status_t
_and_wait(lco_t *lco) {
  _and_t *and = (_and_t *)lco;
  _lock(and);
  hpx_status_t status = _wait(and);
  _unlock(and);
  return status;
}


static hpx_status_t
_and_get(lco_t *lco, int size, void *out) {
  return _and_wait(lco);
}


static void
_and_init(_and_t *and, int64_t value) {
  static const lco_class_t vtable = { _and_fini, _and_error, _and_set, _and_get,
                                      _and_wait };
  assert(value >= 0);
  lco_init(&and->vtable, &vtable, 0);
  and->value = value;
  if (value != 0)
    return;

  _lock(and);
  scheduler_signal(&and->barrier, HPX_SUCCESS);
  _unlock(and);
}

/// @}



/// ----------------------------------------------------------------------------
/// Allocate a and LCO. This is synchronous.
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_lco_and_new(uint64_t limit) {
  hpx_addr_t target;
  _and_t *and = _free_ands;
  if (and) {
    _free_ands = (_and_t*)and->vtable;
    target = HPX_HERE;
    char *base;
    if (!hpx_gas_try_pin(target, (void**)&base))
      hpx_abort();
    target.offset = (char*)and - base;
    assert(target.offset < target.block_bytes);
  }
  else {
    target = hpx_gas_alloc(sizeof(_and_t));
    if (!hpx_gas_try_pin(target, (void**)&and))
      hpx_abort();
  }

  _and_init(and, (int64_t)limit);
  hpx_gas_unpin(target);
  return target;
}


/// ----------------------------------------------------------------------------
/// Join the and.
/// ----------------------------------------------------------------------------
void
hpx_lco_and_set(hpx_addr_t and, hpx_addr_t sync) {
  hpx_lco_set(and, NULL, 0, sync);
}
