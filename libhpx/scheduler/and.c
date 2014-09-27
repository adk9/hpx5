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

/// @file libhpx/scheduler/and.c
/// Defines the AND LCO.

#include <assert.h>
#include <stdint.h>

#include "hpx/hpx.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "lco.h"
#include "cvar.h"


/// And LCO class interface.
/// @{
typedef struct {
  lco_t                  lco;
  cvar_t             barrier;
  volatile intptr_t value;                  // the threshold
} _and_t;


// Freelist
static __thread _and_t *_free_ands = NULL;


static hpx_status_t
_wait(_and_t *and)
{
  // and->value can change asynchronously, but and->lco and and->barrier do not
  // because we're holding the lock here
  hpx_status_t status = cvar_get_error(&and->barrier);
  if (status != HPX_SUCCESS)
    return status;

  // read the value using an atomic, we either see 0---in which case either the
  // and has already been signaled, or there is someone trying to get the lock
  // we hold in order to signal it. In both cases, the and has been satisfied,
  // and we can correctly return the status that we read (the status won't
  // change, because that's done through the hpx_lco_error() handler which would
  // need the lock too).
  intptr_t val = sync_load(&and->value, SYNC_ACQUIRE);
  if (val == 0)
    return status;
  
  // otherwise wait for the and to be signaled
  return scheduler_wait(&and->lco.lock, &and->barrier);
}

static hpx_status_t
_try_wait(_and_t *and, hpx_time_t time)
{
  // and->value can change asynchronously, but and->lco and and->barrier do not
  // because we're holding the lock here
  hpx_status_t status = cvar_get_error(&and->barrier);
  if (status != HPX_SUCCESS)
    return status;
  
  // read the value using an atomic, we either see 0---in which case either the
  // and has already been signaled, or there is someone trying to get the lock
  // we hold in order to signal it. In both cases, the and has been satisfied,
  // and we can correctly return the status that we read (the status won't
  // change, because that's done through the hpx_lco_error() handler which would
  // need the lock too).
  intptr_t val = sync_load(&and->value, SYNC_ACQUIRE);
  if (val == 0)
    return status;
  
  // otherwise wait for the and barrier to reach 0 or return if out of time
  bool timeout = false;
  while (val != 0 && !timeout) {
    if (hpx_time_diff_us(hpx_time_now(), time) > 0) {
      timeout = true;
      break;
    }
    hpx_thread_yield();
    val = sync_load(&and->value, SYNC_ACQUIRE);
  }

  if (!timeout)
    return HPX_SUCCESS;
  else
    return HPX_LCO_TIMEOUT;
}

static void
_free(_and_t *and) {
  // repurpose value as a freelist "next" pointer
  and->value = (intptr_t)_free_ands;
  _free_ands = and;
}


static void
_and_fini(lco_t *lco)
{
  if (!lco)
    return;

  _and_t *and = (_and_t *)lco;
  lco_lock(&and->lco);
  _free(and);
}


static void
_and_error(lco_t *lco, hpx_status_t code)
{
  _and_t *and = (_and_t *)lco;
  lco_lock(&and->lco);
  scheduler_signal_error(&and->barrier, code);
  lco_unlock(&and->lco);
}

/// Fast set uses atomic ops to decrement the value, and signals when it gets to 0.
static void
_and_set(lco_t *lco, int size, const void *from)
{
  _and_t *and = (_and_t *)lco;
  intptr_t val = sync_fadd(&and->value, -1, SYNC_ACQ_REL);

  if (val > 1)
    return;

  if (val < 1) {
    dbg_error("and: too many threads joined the AND lco.\n");
    return;
  }

  lco_lock(&and->lco);
  scheduler_signal_all(&and->barrier);
  lco_unlock(&and->lco);
}


static hpx_status_t
_and_wait(lco_t *lco) {
  _and_t *and = (_and_t *)lco;
  lco_lock(&and->lco);
  hpx_status_t status = _wait(and);
  lco_unlock(&and->lco);
  return status;
}

static hpx_status_t
_and_try_wait(lco_t *lco, hpx_time_t time) {
  _and_t *and = (_and_t *)lco;
  lco_lock(&and->lco);
  hpx_status_t status = _try_wait(and, time);
  lco_unlock(&and->lco);
  return status;
}

static hpx_status_t
_and_get(lco_t *lco, int size, void *out) {
  return _and_wait(lco);
}

static hpx_status_t
_and_try_get(lco_t *lco, int size, void *out, hpx_time_t time) {
  return _and_try_wait(lco, time);
}

static void
_and_init(_and_t *and, intptr_t value) {
  static const lco_class_t vtable = {
    .on_fini = _and_fini,
    .on_error = _and_error,
    .on_set = _and_set,
    .on_get = _and_get,
    .on_wait = _and_wait,
    .on_try_get = _and_try_get,
    .on_try_wait = _and_try_wait
  };

  assert(value >= 0);
  lco_init(&and->lco, &vtable, 0);
  cvar_reset(&and->barrier);
  and->value = value;
}

/// @}



/// Allocate an and LCO. This is synchronous.
hpx_addr_t
hpx_lco_and_new(intptr_t limit) {
  hpx_addr_t target;
  _and_t *and = _free_ands;
  if (and) {
    _free_ands = (_and_t*)and->value;
    target = HPX_HERE;
    char *base;
    if (!hpx_gas_try_pin(target, (void**)&base))
      hpx_abort();
    target.offset = (char*)and - base;
    assert(target.offset < target.block_bytes);
  }
  else {
    target = locality_malloc(sizeof(_and_t));
    if (!hpx_gas_try_pin(target, (void**)&and))
      hpx_abort();
  }

  _and_init(and, limit);
  hpx_gas_unpin(target);
  return target;
}


/// Join the and.
void
hpx_lco_and_set(hpx_addr_t and, hpx_addr_t rsync) {
  hpx_lco_set(and, 0, NULL, HPX_NULL, rsync);
}
