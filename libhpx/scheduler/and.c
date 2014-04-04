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
#include "libhpx/scheduler.h"
#include "lco.h"


/// ----------------------------------------------------------------------------
/// And LCO class interface.
/// ----------------------------------------------------------------------------
/// @{
typedef struct {
  lco_t lco;                                    // and "is-an" lco
  SYNC_ATOMIC(uint64_t value);                  // the threshold
} _and_t;


// Freelist
static __thread _and_t *_free = NULL;


static void _delete(_and_t *and) {
  if (!and)
    return;
  lco_lock(&and->lco);
  lco_fini(&and->lco);
  and->lco.vtable = (lco_class_t*)_free;
  _free = and;
}


/// Fast set uses atomic ops to decrement the value, and signals when it gets to 0.
static void _set(_and_t *and, int size, const void *from) {
  uint64_t val; sync_load(val, &and->value, SYNC_ACQUIRE);
  if (val == 0)
    return;

  if (!sync_cas(&and->value, val, val - 1, SYNC_RELEASE, SYNC_RELAXED))
    _set(and, size, from);

  if (val - 1 != 0)
    return;

  lco_lock(&and->lco);
  scheduler_signal(&and->lco);
  lco_unlock(&and->lco);
}


/// Basic get functionality.
static void _get(_and_t *and, int size, void *out) {
  lco_lock(&and->lco);
  if (!lco_is_set(&and->lco))
    scheduler_wait(&and->lco);
  lco_unlock(&and->lco);
}


/// The AND vtable.
static lco_class_t _vtable = LCO_CLASS_INIT(_delete, _set, _get);


static void _init(_and_t *and, uint64_t value) {
  lco_init(&and->lco, &_vtable, 0);
  and->value = value;
}

/// @}



/// ----------------------------------------------------------------------------
/// Allocate a and LCO. This is synchronous.
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_lco_and_new(uint64_t limit) {
  hpx_addr_t target;
  _and_t *and = _free;
  if (and) {
    _free = (_and_t*)and->lco.vtable;
    target = HPX_HERE;
    char *base;
    if (!hpx_addr_try_pin(target, (void**)&base))
      hpx_abort();
    target.offset = (char*)and - base;
    assert(target.offset < target.block_bytes);
  }
  else {
    target = hpx_alloc(sizeof(_and_t));
    if (!hpx_addr_try_pin(target, (void**)&and))
      hpx_abort();
  }

  _init(and, limit);
  hpx_addr_unpin(target);
  return target;
}


/// ----------------------------------------------------------------------------
/// Join the and.
/// ----------------------------------------------------------------------------
void
hpx_lco_and_set(hpx_addr_t and, hpx_addr_t sync) {
  hpx_lco_set(and, NULL, 0, sync);
}
