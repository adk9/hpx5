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

/// @file libhpx/scheduler/and.c
/// Defines the AND LCO.

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/scheduler.h>
#include "cvar.h"
#include "lco.h"

/// And LCO class interface.
/// @{
typedef struct {
  lco_t               lco;
  cvar_t          barrier;
  volatile intptr_t value;                  // the threshold
} _and_t;

static hpx_status_t _wait(_and_t *and) {
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

static hpx_status_t _attach(_and_t *and, hpx_parcel_t *p) {
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
  }
}

static void _and_error(lco_t *lco, hpx_status_t code) {
  _and_t *and = (_and_t *)lco;
  lco_lock(&and->lco);
  scheduler_signal_error(&and->barrier, code);
  lco_unlock(&and->lco);
}

void _and_reset(lco_t *lco) {
  _and_t *and = (_and_t *)lco;
  lco_lock(&and->lco);
  dbg_assert_str(cvar_empty(&and->barrier),
                 "Reset on AND LCO that has waiting threads.\n");
  cvar_reset(&and->barrier);
  lco_unlock(&and->lco);
}

/// Fast set decrements the value, and signals when it gets to 0.
static void _and_set(lco_t *lco, int size, const void *from) {
  dbg_assert_str(lco != NULL, "lco-set on a NULL lco.\n");

  _and_t *and = (_and_t *)lco;
  int num = (size && from) ? *(int*)from : 1;
  intptr_t value = sync_addf(&and->value, -num, SYNC_ACQ_REL);
  log_lco("reduced count to %" PRIdPTR " lco %p\n", value, (void*)&and->lco);

  if (value == 0) {
    lco_lock(&and->lco);
    scheduler_signal_all(&and->barrier);
    lco_unlock(&and->lco);
  }
  else {
    dbg_assert_str(value > 0, "too many threads joined (%ld).\n", value);
  }
}

static size_t _and_size(lco_t *lco) {
  _and_t *and = (_and_t *)lco;
  return sizeof(*and);
}

static hpx_status_t _and_wait(lco_t *lco) {
  _and_t *and = (_and_t *)lco;
  lco_lock(&and->lco);
  hpx_status_t status = _wait(and);
  lco_unlock(&and->lco);
  return status;
}

static hpx_status_t _and_attach(lco_t *lco, hpx_parcel_t *p) {
  _and_t *and = (_and_t *)lco;
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
  .on_reset    = _and_reset,
  .on_size     = _and_size
};

static int _and_init(_and_t *and, int64_t value) {
  assert(value >= 0);
  lco_init(&and->lco, &_and_vtable);
  cvar_reset(&and->barrier);
  sync_store(&and->value, value, SYNC_RELEASE);
  log_lco("initialized with %" PRId64 " inputs lco %p\n", (int64_t)and->value, (void*)and);
  return HPX_SUCCESS;
}
static HPX_ACTION_DEF(PINNED, _and_init, _and_init_async, HPX_SINT64);

/// @}

/// Allocate an and LCO. This is synchronous.
hpx_addr_t hpx_lco_and_new(int64_t limit) {
  _and_t *and = NULL;
  hpx_addr_t gva = hpx_gas_alloc_local(sizeof(*and), 0);
  LCO_LOG_NEW(gva);

  if (!hpx_gas_try_pin(gva, (void**)&and)) {
    int e = hpx_call_sync(gva, _and_init_async, NULL, 0, &limit);
    dbg_check(e, "could not initialize an and gate at %lu\n", gva);
  }
  else {
    _and_init(and, limit);
    hpx_gas_unpin(gva);
  }
  return gva;
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

/// Initialize a block of array of and lco.
static HPX_PINNED(_block_local_init, void *lco, uint32_t *args) {
  for (int i = 0; i < args[0]; i++) {
    void *addr = (void *)((uintptr_t)lco + i * sizeof(_and_t));
    _and_init(addr, (intptr_t)args[1]);
  }
  return HPX_SUCCESS;
}

/// Allocate an array of and LCO local to the calling locality.
/// @param          n The (total) number of lcos to allocate
/// @param     inputs number of inputs to the and (must be >= 0)
///
/// @returns the global address of the allocated array lco.
hpx_addr_t hpx_lco_and_local_array_new(int n, int arg) {
  // Get the sizeof lco class structure
  uint32_t lco_bytes = sizeof(_and_t);
  dbg_assert(n * lco_bytes < UINT32_MAX);
  uint32_t  block_bytes = n * lco_bytes;
  hpx_addr_t base = hpx_gas_alloc_local(block_bytes, 0);

  uint32_t args[] = {n, arg};
  int e = hpx_call_sync(base, _block_local_init, NULL, 0, &args, sizeof(args));
  dbg_check(e, "call of _block_init_action failed\n");

  // return the base address of the allocation
  return base;
}

