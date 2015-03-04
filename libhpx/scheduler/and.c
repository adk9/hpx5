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

static hpx_status_t _wait(and_t *and) {
  hpx_status_t status = cvar_get_error(&and->barrier);
  if (status != HPX_SUCCESS) {
    return status;
  }

  intptr_t value = sync_load(&and->value, SYNC_ACQUIRE);
  if (value == 0) {
    return status;
  }

  // otherwise wait for the and to be signaled
  log_lco("waiting for lco %p\n", (void*)&and->lco);
  return scheduler_wait(&and->lco.lock, &and->barrier);
}

static hpx_status_t _attach(and_t *and, hpx_parcel_t *p) {
  hpx_status_t status = cvar_get_error(&and->barrier);
  if (status != HPX_SUCCESS) {
    return status;
  }

  intptr_t value = sync_load(&and->value, SYNC_ACQUIRE);
  if (value == 0) {
    return hpx_parcel_send(p, HPX_NULL);
  }

  return cvar_attach(&and->barrier, p);
}

static void _and_fini(lco_t *lco) {
  if (lco) {
    lco_lock(lco);
    lco_fini(lco);
    libhpx_global_free(lco);
  }
}

static void _and_error(lco_t *lco, hpx_status_t code) {
  and_t *and = (and_t *)lco;
  lco_lock(&and->lco);
  scheduler_signal_error(&and->barrier, code);
  lco_unlock(&and->lco);
}

void _and_reset(lco_t *lco) {
  and_t *and = (and_t *)lco;
  lco_lock(&and->lco);
  dbg_assert_str(cvar_empty(&and->barrier),
                 "Reset on AND LCO that has waiting threads.\n");
  cvar_reset(&and->barrier);
  lco_unlock(&and->lco);
}

/// Fast set decrements the value, and signals when it gets to 0.
static void _and_set(lco_t *lco, int size, const void *from) {
  dbg_assert_str(lco != NULL, "lco-set on a NULL lco.\n");

  and_t *and = (and_t *)lco;
  int num = (size && from) ? *(int*)from : 1;
  intptr_t value = sync_addf(&and->value, -num, SYNC_ACQ_REL);
  log_lco("reduced count to %ld lco %p\n", value, (void*)&and->lco);

  if (value == 0) {
    lco_lock(&and->lco);
    scheduler_signal_all(&and->barrier);
    lco_unlock(&and->lco);
  }
  else {
    dbg_assert_str(value > 0, "too many threads joined (%ld).\n", value);
  }
}

static hpx_status_t _and_wait(lco_t *lco) {
  and_t *and = (and_t *)lco;
  lco_lock(&and->lco);
  hpx_status_t status = _wait(and);
  lco_unlock(&and->lco);
  return status;
}

static hpx_status_t _and_attach(lco_t *lco, hpx_parcel_t *p) {
  and_t *and = (and_t *)lco;
  lco_lock(&and->lco);
  hpx_status_t status = _attach(and, p);
  lco_unlock(&and->lco);
  return status;
}

static hpx_status_t _and_get(lco_t *lco, int size, void *out) {
  return _and_wait(lco);
}

static const lco_class_t _and_vtable = {
  .on_fini     = _and_fini,
  .on_error    = _and_error,
  .on_set      = _and_set,
  .on_get      = _and_get,
  .on_getref   = NULL,
  .on_release  = NULL,
  .on_wait     = _and_wait,
  .on_attach   = _and_attach,
  .on_reset    = _and_reset
};

void and_init(and_t *and, intptr_t value) {
  assert(value >= 0);
  lco_init(&and->lco, &_and_vtable);
  cvar_reset(&and->barrier);
  sync_store(&and->value, value, SYNC_RELEASE);
  log_lco("initialized with %ld inputs lco %p\n", and->value, (void*)and);
}

/// @}


/// Allocate an and LCO. This is synchronous.
hpx_addr_t hpx_lco_and_new(intptr_t limit) {
  and_t *and = libhpx_global_malloc(sizeof(*and));
  dbg_assert_str(and, "Could not malloc global memory\n");
  log_lco("allocated lco %p\n", (void*)and);
  and_init(and, limit);
  return lva_to_gva(and);;
}


/// Join the and.
void hpx_lco_and_set(hpx_addr_t and, hpx_addr_t rsync) {
  hpx_lco_set(and, 0, NULL, HPX_NULL, rsync);
}

/// Set an and "num" times.
void hpx_lco_and_set_num(hpx_addr_t and, int sum, hpx_addr_t rsync) {
  hpx_addr_t lsync = hpx_lco_future_new(0);
  hpx_lco_set(and, sizeof(sum), &sum, lsync, rsync);
  hpx_lco_wait(lsync);
  hpx_lco_delete(lsync, HPX_NULL);
}
