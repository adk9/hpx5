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

/// @file libhpx/scheduler/future.c
/// Defines the future structure.

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <hpx/builtins.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/scheduler.h>
#include "lco.h"
#include "cvar.h"
#include "future.h"

/// Local future interface.
/// @{
typedef struct {
  lco_t     lco;
  cvar_t   full;
  char  value[];
} _future_t;

static size_t _future_size(lco_t *lco) {
  _future_t *future = (_future_t *)lco;
  return sizeof(*future);
}

static hpx_status_t _wait(_future_t *f) {
  if (!lco_get_triggered(&f->lco))
    return scheduler_wait(&f->lco.lock, &f->full);

  return cvar_get_error(&f->full);
}

static bool _trigger(_future_t *f) {
  if (lco_get_triggered(&f->lco))
    return false;
  lco_set_triggered(&f->lco);
  return true;
}

// Nothing extra allocated in the future
static void _future_fini(lco_t *lco) {
  if (!lco) {
    return;
  }

  lco_lock(lco);
  lco_fini(lco);
  global_free(lco);
}

/// Copies @p from into the appropriate location.
static void _future_set(lco_t *lco, int size, const void *from) {
  lco_lock(lco);
  _future_t *f = (_future_t *)lco;
  // futures are write-once
  if (!_trigger(f)) {
    dbg_error("cannot set an already set future\n");
    goto unlock;
  }

  if (from && size) {
    memcpy(&f->value, from, size);
  }

  scheduler_signal_all(&f->full);

 unlock:
  lco_unlock(lco);
}

void lco_future_set(lco_t *lco, int size, const void *from) {
  _future_set(lco, size, from);
}

static void _future_error(lco_t *lco, hpx_status_t code) {
  lco_lock(lco);
  _future_t *f = (_future_t *)lco;
  _trigger(f);
  scheduler_signal_error(&f->full, code);
  lco_unlock(lco);
}

static void _future_reset(lco_t *lco) {
  _future_t *f = (_future_t *)lco;
  lco_lock(&f->lco);
  dbg_assert_str(cvar_empty(&f->full),
                 "Reset on a future that has waiting threads.\n");
  lco_reset_triggered(&f->lco);
  cvar_reset(&f->full);
  lco_unlock(&f->lco);
}

static hpx_status_t _future_attach(lco_t *lco, hpx_parcel_t *p) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _future_t *f = (_future_t *)lco;

  // if the future isn't triggered, then attach this parcel to the full
  // condition
  if (!lco_get_triggered(lco)) {
    status = cvar_attach(&f->full, p);
    goto unlock;
  }

  // if the future has an error, then return that error without sending the
  // parcel
  //
  // NB: should we actually send some sort of error condition?
  status = cvar_get_error(&f->full);
  if (status != HPX_SUCCESS) {
    goto unlock;
  }

  // go ahead and send this parcel eagerly
  hpx_parcel_send(p, HPX_NULL);

 unlock:
  lco_unlock(lco);
  return status;
}

/// Copies the appropriate value into @p out, waiting if the lco isn't set yet.
static hpx_status_t _future_get(lco_t *lco, int size, void *out) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);

  _future_t *f = (_future_t *)lco;
  status = _wait(f);
  if ((status == HPX_SUCCESS) && out) {
    memcpy(out, &f->value, size);
  }

  lco_unlock(lco);
  return status;
}

/// Returns the reference to the future's value in @p out, waiting if
/// the lco isn't set yet.
static hpx_status_t _future_getref(lco_t *lco, int size, void **out) {
  _future_t *f = (_future_t *)lco;
  lco_lock(&f->lco);
  hpx_status_t status = _wait(f);

  if ((status == HPX_SUCCESS) && out) {
    *out = &f->value;
  }

  lco_unlock(&f->lco);
  return status;
}

/// Free the reference to the future's value. If the future was
/// _moved_ to our locality after a getref, check if the reference to
/// be released matches the reference to the future's value.
static bool _future_release(lco_t *lco, void *out) {
  bool ret = false;
  _future_t *f = (_future_t *)lco;
  lco_lock(&f->lco);
  if (out && out != f->value) {
    free(out);
    ret = true;
  }
  lco_unlock(&f->lco);
  return ret;
}

static hpx_status_t _future_wait(lco_t *lco) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  _future_t *f = (_future_t *)lco;
  status = _wait(f);
  lco_unlock(lco);
  return status;
}

/// initialize the future
static void _future_init(_future_t *f, int size) {
  // the future vtable
  static const lco_class_t vtable = {
    .on_fini     = _future_fini,
    .on_error    = _future_error,
    .on_set      = _future_set,
    .on_get      = _future_get,
    .on_getref   = _future_getref,
    .on_release  = _future_release,
    .on_wait     = _future_wait,
    .on_attach   = _future_attach,
    .on_reset    = _future_reset,
    .on_size     = _future_size
  };

  lco_init(&f->lco, &vtable);
  cvar_reset(&f->full);
  if (size) {
    memset(&f->value, 0, size);
  }
}

/// Initialize a block of futures.
static HPX_PINNED(_block_init, char *base, uint32_t *args) {
  const uint32_t size = args[0];
  const uint32_t nfutures = args[1];

  // sequentially initialize each future
  for (uint32_t i = 0; i < nfutures; ++i) {
    _future_init((_future_t*)(base + i * size), size);
  }
  return HPX_SUCCESS;
}

hpx_addr_t hpx_lco_future_new(int size) {
  _future_t *local = global_malloc(sizeof(*local) + size);
  dbg_assert(local);
  _future_init(local, size);
  return lva_to_gva(local);
}

// Allocate a global array of futures.
hpx_addr_t hpx_lco_future_array_new(int n, int size, int futures_per_block) {
  // perform the global allocation
  uint32_t       blocks = ceil_div_32(n, futures_per_block);
  uint32_t future_bytes = sizeof(_future_t) + size;
  uint32_t  block_bytes = futures_per_block * future_bytes;
  hpx_addr_t       base = hpx_gas_alloc_cyclic(blocks, block_bytes);

  // for each block, initialize the future
  uint32_t args[] = { size, futures_per_block };

  hpx_addr_t and = hpx_lco_and_new(blocks);
  for (int i = 0; i < blocks; ++i) {
    hpx_addr_t there = hpx_addr_add(base, i * block_bytes, block_bytes);
    int e = hpx_call(there, _block_init, and, args, sizeof(args));
    dbg_check(e, "call of _block_init_action failed\n");
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  // return the base address of the allocation
  return base;
}


// Application level programmer doesn't know how big the future is, so we
// provide this array indexer.
hpx_addr_t hpx_lco_future_array_at(hpx_addr_t array, int i, int size, int bsize)
{
  uint32_t future_bytes = sizeof(_future_t) + size;
  uint32_t  block_bytes = bsize * future_bytes;
  return hpx_addr_add(array, i * (sizeof(_future_t) + size), block_bytes);
}

/// Initialize a block of array of lco.
static HPX_PINNED(_block_local_init, void *lco, uint32_t *args) {
  for (int i = 0; i < args[0]; i++) {
    void *addr = (void *)((uintptr_t)lco + i * (sizeof(_future_t) + args[1]));
    _future_init(addr, args[1]);
  }
  return HPX_SUCCESS;
}

/// Allocate an array of future local to the calling locality.
/// @param          n The (total) number of futures to allocate
/// @param       size The size of each future's value 
///
/// @returns the global address of the allocated array lco.
hpx_addr_t hpx_lco_future_local_array_new(int n, int size) {
  uint32_t lco_bytes = sizeof(_future_t) + size;
  dbg_assert(n * lco_bytes < UINT32_MAX);
  uint32_t  block_bytes = n * lco_bytes;
  hpx_addr_t base = hpx_gas_alloc_local(block_bytes);

  // for each block, initialize the future.
  uint32_t args[] = {n, size};
  int e = hpx_call_sync(base, _block_local_init, NULL, 0, &args, sizeof(args));
  dbg_check(e, "call of _block_init_action failed\n");

  return base;  
}
