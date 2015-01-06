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

/// @file libhpx/scheduler/future.c
/// Defines the future structure.

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <hpx/builtins.h>
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "lco.h"
#include "cvar.h"

/// Local future interface.
/// @{
typedef struct {
  lco_t     lco;
  cvar_t   full;
  char  value[];
} _future_t;

static hpx_status_t _wait(_future_t *f) {
  if (!lco_get_triggered(&f->lco))
    return scheduler_wait(&f->lco.lock, &f->full);

  return cvar_get_error(&f->full);
}

static hpx_status_t _try_wait(_future_t *f, hpx_time_t time) {
  while (!lco_get_triggered(&f->lco)) {
    if (hpx_time_diff_us(hpx_time_now(), time) > 0)
      return HPX_LCO_TIMEOUT;
    hpx_thread_yield();
  }
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
  _future_t *f = (void*)lco;
  if (f)
    lco_lock(&f->lco);
  libhpx_global_free(f);
}

/// Copies @p from into the appropriate location.
static void _future_set(lco_t *lco, int size, const void *from)
{
  _future_t *f = (_future_t *)lco;
  lco_lock(&f->lco);

  // futures are write-once
  if (!_trigger(f))
    goto unlock;

  if (from && size)
    memcpy(&f->value, from, size);

  scheduler_signal_all(&f->full);

 unlock:
  lco_unlock(&f->lco);
}

void lco_future_set(lco_t *lco, int size, const void *from) {
  _future_set(lco, size, from);
}

static void _future_error(lco_t *lco, hpx_status_t code) {
  _future_t *f = (_future_t *)lco;
  lco_lock(&f->lco);
  scheduler_signal_error(&f->full, code);
  lco_unlock(&f->lco);
}

static void _future_reset(_future_t *f) {
  lco_lock(&f->lco);
  scheduler_signal_error(&f->full, HPX_LCO_RESET);
  cvar_reset(&f->full);
  lco_unlock(&f->lco);
}

/// Copies the appropriate value into @p out, waiting if the lco isn't set yet.
static hpx_status_t _future_get(lco_t *lco, int size, void *out) {
  _future_t *f = (_future_t *)lco;
  lco_lock(&f->lco);
  hpx_status_t status = _wait(f);

  if ((status == HPX_SUCCESS) && out)
    memcpy(out, &f->value, size);

  lco_unlock(&f->lco);
  return status;
}

static hpx_status_t _future_wait(lco_t *lco) {
  _future_t *f = (_future_t *)lco;
  lco_lock(&f->lco);
  hpx_status_t status = _wait(f);
  lco_unlock(&f->lco);
  return status;
}

static hpx_status_t _future_try_wait(lco_t *lco, hpx_time_t time) {
  _future_t *f = (_future_t *)lco;
  lco_lock(&f->lco);
  hpx_status_t status = _try_wait(f, time);
  lco_unlock(&f->lco);
  return status;
}

static int _future_reset_handler(void *UNUSED) {
  hpx_addr_t target = hpx_thread_current_target();
  _future_t *f = NULL;
  if (!hpx_gas_try_pin(target, (void**)&f))
    return HPX_RESEND;

  _future_reset(f);
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

/// initialize the future
static void _future_init(_future_t *f, int size) {
  // the future vtable
  static const lco_class_t vtable = {
    .on_fini = _future_fini,
    .on_error = _future_error,
    .on_set = _future_set,
    .on_get = _future_get,
    .on_wait = _future_wait,
    //    .on_try_get = _future_try_get,
    .on_try_wait = _future_try_wait
  };

  lco_init(&f->lco, &vtable, 0);
  cvar_reset(&f->full);
  if (size)
    memset(&f->value, 0, size);
}

/// Initialize a block of futures.
static int _future_block_init_handler(uint32_t *args) {
  const uint32_t size = args[0];
  const uint32_t nfutures = args[1];

  hpx_addr_t target = hpx_thread_current_target();
  char *base = NULL;

  // application level forwarding if the future block has moved
  if (!hpx_gas_try_pin(target, (void**)&base))
    return HPX_RESEND;

  // sequentially initialize each future
  for (uint32_t i = 0; i < nfutures; ++i)
    _future_init((_future_t*)(base + i * size), size);

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

static hpx_action_t _block_init = 0;
static hpx_action_t _future_reset_action = 0;

static void HPX_CONSTRUCTOR _future_initialize_actions(void) {
  LIBHPX_REGISTER_ACTION(&_block_init, _future_block_init_handler);
  LIBHPX_REGISTER_ACTION(&_future_reset_action, _future_reset_handler);
}


hpx_addr_t hpx_lco_future_new(int size) {
  _future_t *local = libhpx_global_malloc(sizeof(*local) + size);
  assert(local);
  _future_init(local, size);
  return lva_to_gva(local);
}

void hpx_lco_future_reset(hpx_addr_t future, hpx_addr_t sync) {
  _future_t *f;
  if (!hpx_gas_try_pin(future, (void**)&f)) {
    hpx_call_async(future, _future_reset_action, NULL, 0, HPX_NULL, sync);
    return;
  }

  _future_reset(f);
  hpx_gas_unpin(future);
  if (sync)
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
}


// Allocate a global array of futures.
hpx_addr_t hpx_lco_future_array_new(int n, int size, int futures_per_block) {
  // perform the global allocation
  uint32_t       blocks = ceil_div_32(n, futures_per_block);
  uint32_t future_bytes = sizeof(_future_t) + size;
  uint32_t  block_bytes = futures_per_block * future_bytes;
  hpx_addr_t       base = hpx_gas_global_alloc(blocks, block_bytes);

  // for each block, initialize the future
  uint32_t args[] = { size, futures_per_block };

  hpx_addr_t and = hpx_lco_and_new(blocks);
  for (int i = 0; i < blocks; ++i) {
    hpx_addr_t there = hpx_addr_add(base, i * block_bytes, block_bytes);
    int e = hpx_call(there, _block_init, args, sizeof(args), and);
    dbg_check(e, "call of _block_init failed\n");
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


