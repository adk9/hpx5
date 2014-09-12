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

#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "lco.h"
#include "cvar.h"

/// Local future interface.
/// @{
typedef struct {
  lco_t    lco;                                 //
  cvar_t  full;                                 //
  void  *value;                                 // 1 in-place word
} _future_t;


/// Freelist allocation for futures.
static __thread _future_t *_free_futures = NULL;


/// Remote block initialization
static hpx_action_t _future_block_init = 0;
static hpx_action_t _future_blocks_init = 0;


/// Use the LCO's "user" state to remember if the future is in-place or not.
static uintptr_t
_is_inplace(const _future_t *f) {
  return lco_get_user(&f->lco);
}


static hpx_status_t
_wait(_future_t *f) {
  if (!lco_get_triggered(&f->lco))
    return scheduler_wait(&f->lco.lock, &f->full);

  return cvar_get_error(&f->full);
}


static bool
_trigger(_future_t *f) {
  if (lco_get_triggered(&f->lco))
    return false;
  lco_set_triggered(&f->lco);
  return true;
}


static void
_free(_future_t *f) {
  // overload the value for freelisting---not perfect
  f->value = _free_futures;
  _free_futures = f;
}


/// Deletes the future's out of place data, if necessary.
///
/// NB: deadlock issue here
static void
_future_fini(lco_t *lco)
{
  if (!lco)
    return;

  _future_t *f = (_future_t *)lco;
  lco_lock(&f->lco);

  if (!_is_inplace(f)) {
    void *ptr = NULL;
    memcpy(&ptr, &f->value, sizeof(ptr));       // strict aliasing
    free(ptr);
  }

  _free(f);
}


/// Copies @p from into the appropriate location. Futures are single-assignment,
/// so we only do this if the future isn't set yet.
static void
_future_set(lco_t *lco, int size, const void *from)
{
  _future_t *f = (_future_t *)lco;
  lco_lock(&f->lco);

  if (!_trigger(f))
    goto unlock;

  if (from && _is_inplace(f)) {
    memcpy(&f->value, from, size);
  }
  else if (from) {
    void *ptr = NULL;
    memcpy(&ptr, &f->value, sizeof(ptr));       // strict aliasing
    memcpy(ptr, from, size);
  }

  scheduler_signal_all(&f->full);

 unlock:
  lco_unlock(&f->lco);
}


static void
_future_error(lco_t *lco, hpx_status_t code)
{
  _future_t *f = (_future_t *)lco;
  lco_lock(&f->lco);
  scheduler_signal_error(&f->full, code);
  lco_unlock(&f->lco);
}


/// Copies the appropriate value into @p out, waiting if the lco isn't set yet.
static hpx_status_t
_future_get(lco_t *lco, int size, void *out)
{
  _future_t *f = (_future_t *)lco;
  lco_lock(&f->lco);
  hpx_status_t status = _wait(f);

  if (status != HPX_SUCCESS)
    goto unlock;

  if (out && _is_inplace(f)) {
    memcpy(out, &f->value, size);
    goto unlock;
  }

  if (out) {
    void *ptr = NULL;
    memcpy(&ptr, &f->value, sizeof(ptr));       // strict aliasing
    memcpy(out, ptr, size);
  }

 unlock:
  lco_unlock(&f->lco);
  return status;
}

static hpx_status_t
_future_wait(lco_t *lco)
{
  _future_t *f = (_future_t *)lco;
  lco_lock(&f->lco);
  hpx_status_t status = _wait(f);
  lco_unlock(&f->lco);
  return status;
}


/// initialize the future
static void
_future_init(_future_t *f, int size)
{
  // the future vtable
  static const lco_class_t vtable = {
    _future_fini,
    _future_error,
    _future_set,
    _future_get,
    _future_wait
  };

  bool inplace = (size <= sizeof(f->value));
  lco_init(&f->lco, &vtable, inplace);
  cvar_reset(&f->full);
  if (!inplace) {
    f->value = malloc(size);                    // allocate if necessary
    assert(f->value);
  }
  else {
    memset(&f->value, 0, sizeof(f->value));
  }
}


/// Initialize a block of futures.
static int
_future_block_init_action(uint32_t *args) {
  hpx_addr_t target = hpx_thread_current_target();
  _future_t *futures = NULL;

  // application level forwarding if the future block has moved
  if (!hpx_gas_try_pin(target, (void**)&futures))
    return HPX_RESEND;

  // sequentially initialize each future
  uint32_t size = args[0];
  uint32_t block_size = args[1];
  for (uint32_t i = 0; i < block_size; ++i)
    _future_init(&futures[i], size);

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


/// Initialize a strided block of futures
static int
_future_blocks_init_action(uint32_t *args) {
  hpx_addr_t      base = hpx_thread_current_target();
  uint32_t  block_size = args[1];
  uint32_t block_bytes = block_size * sizeof(_future_t);
  uint32_t      blocks = args[2];

  hpx_addr_t and = hpx_lco_and_new(blocks);
  for (uint32_t i = 0; i < blocks; i++) {
    hpx_addr_t block = hpx_addr_add(base, i * here->ranks * block_bytes);
    hpx_call(block, _future_block_init, args, 2 * sizeof(*args), and);
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  return HPX_SUCCESS;
}


static void HPX_CONSTRUCTOR
_future_initialize_actions(void) {
  _future_block_init  = HPX_REGISTER_ACTION(_future_block_init_action);
  _future_blocks_init = HPX_REGISTER_ACTION(_future_blocks_init_action);
}


hpx_addr_t
hpx_lco_future_new(int size) {
  hpx_addr_t f;
  _future_t *local = _free_futures;
  if (local) {
    _free_futures = (_future_t*)local->value;
    f = HPX_HERE;
    char *base;
    if (!hpx_gas_try_pin(f, (void**)&base)) {
      dbg_error("future: could not translate local block.\n");
    }
    f.offset = (char*)local - base;
    assert(f.offset < f.block_bytes);
  }
  else {
    f = locality_malloc(sizeof(_future_t));
    if (!hpx_gas_try_pin(f, (void**)&local)) {
      dbg_error("future: could not pin newly allocated future of size %d.\n", size);
      hpx_abort();
    }
  }
  _future_init(local, size);
  hpx_gas_unpin(f);
  return f;
}


// Allocate a global array of futures.
//
// Each of the futures needs to be initialized correctly, and if they need to
// be out of place, then each locality needs to allocate the out-of-place size
// required.
hpx_addr_t
hpx_lco_future_array_new(int n, int size, int block_size) {
  // perform the global allocation
  uint32_t blocks = (n / block_size) + ((n % block_size) ? 1 : 0);
  uint32_t block_bytes = block_size * sizeof(_future_t);
  hpx_addr_t base = hpx_gas_global_alloc(blocks, block_bytes);

  // for each rank, send an initialization message
  uint32_t args[3] = {
    size,
    block_size,
    (blocks / here->ranks) // bks per rank
  };

  // We want to do this in parallel, but wait for them all to complete---we
  // don't need any values from this broadcast, so we can use the and
  // reduction.
  int ranks = here->ranks;
  int rem = blocks % here->ranks;
  hpx_addr_t and[2] = {
    hpx_lco_and_new(ranks),
    hpx_lco_and_new(rem)
  };

  for (int i = 0; i < ranks; ++i) {
    hpx_addr_t there = hpx_addr_add(base, i * block_bytes);
    hpx_call(there, _future_blocks_init, args, sizeof(args), and[0]);
  }

  for (int i = 0; i < rem; ++i) {
    hpx_addr_t block = hpx_addr_add(base, args[2] * ranks + i * block_bytes);
    hpx_call(block, _future_block_init, args, 2 * sizeof(args[0]), and[1]);
  }

  hpx_lco_wait_all(2, and, NULL);
  hpx_lco_delete(and[0], HPX_NULL);
  hpx_lco_delete(and[1], HPX_NULL);

  // return the base address of the allocation
  return base;
}


// Application level programmer doesn't know how big the future is, so we
// provide this array indexer.
hpx_addr_t
hpx_lco_future_array_at(hpx_addr_t array, int i) {
  return hpx_addr_add(array, i * sizeof(_future_t));
}


// This should probably be managed by the LCO superclass.
void
hpx_lco_future_array_delete(hpx_addr_t array, hpx_addr_t sync) {
  dbg_log_lco("future: array delete unimplemented");
  if (!hpx_addr_eq(sync, HPX_NULL))
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
}





typdef struct _newfuture_t {
  lco_t lco;
  uint32_t bits;
};

static _newfuture_init(size_t size) {

  static const lco_class_t vtable = {
    .on_fini = _newfuture_fini,
    .on_error = _newfuture_error,
    .on_set = _newfuture_set,
    .on_get = _newfuture_get,
    .on_wait = _newfuture_wait
  };

  // it's still not clear to me much of the old lco features it makes sense to use
  // with the new futures.

  
}


hpx_addr_t hpx_lco_newfuture_new(size_t size) {
  _newfuture_t *local = NULL;

  hpx_addr_t f = locality_malloc(sizeof(_newfuture_t));
  if (!hpx_gas_try_pin(f, (void**)&local)) {
    dbg_error("newfuture: could not pin newly allocated future of size %d.\n", size);
    hpx_abort();
  }

  _newfuture_init(local, size);

  return f;
}

hpx_addr_t hpx_lco_newfuture_new_all(int num_participants, size_t size_per_participant) {
  return HPX_NULL;
}

hpx_addr_t hpx_lco_newfuture_shared_new(size_t size) {
  return HPX_NULL;
}

hpx_addr_t hpx_lco_newfuture_shared_new_all(int num_participants, size_t size) {
  return HPX_NULL;
}

hpx_addr_t hpx_lco_newfuture_at(hpx_addr_t base, int id) {
  return base;
}

hpx_status_t hpx_lco_newfuture_setat(hpx_addr_t future,  int id, size_t size, void *data,
				     hpx_addr_t lsync_lco, hpx_addr_t rsync_lco) {
  if (!hpx_addr_eq(lsync_lco, HPX_NULL))
    hpx_lco_set(lsync_lco, 0, NULL, HPX_NULL, HPX_NULL);
  if (!hpx_addr_eq(rsync_lco, HPX_NULL))
    hpx_lco_set(rsync_lco, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

hpx_status_t hpx_lco_newfuture_emptyat(hpx_addr_t newfuture,  int id, hpx_addr_t rsync_lco) {
  if (!hpx_addr_eq(rsync_lco, HPX_NULL))
    hpx_lco_set(rsync_lco, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

hpx_addr_t hpx_lco_newfuture_getat(hpx_addr_t future, int id, size_t size, void *value) {
  return HPX_NULL;
}

void hpx_lco_newfuture_get_all(size_t num, hpx_addr_t futures, size_t size,
			       void *values) {
  return;
}

void hpx_lco_newfuture_waitat(hpx_addr_t future, int id, hpx_set_t set) {
  return;
}

hpx_status_t hpx_lco_newfuture_waitat_for(hpx_addr_t future, int id, hpx_set_t set, hpx_time_t for_time) {
  return HPX_SUCCESS;
}

hpx_status_t hpx_lco_newfuture_waitat_until(hpx_addr_t future, int id, hpx_set_t set, hpx_time_t for_time) {
  return HPX_SUCCESS;
}

void hpx_lco_newfuture_wait_all(size_t num, hpx_addr_t newfutures, hpx_set_t set) {
  return;
}

hpx_status_t hpx_lco_newfuture_wait_all_for(size_t num, hpx_addr_t newfutures, 
					    hpx_set_t set, hpx_time_t time) {
  return HPX_SUCCESS;
}

hpx_status_t hpx_lco_newfuture_wait_all_until(size_t num, hpx_addr_t newfutures, 
					      hpx_set_t set, hpx_time_t time) {
  return HPX_SUCCESS;
}

void hpx_lco_newfuture_free(hpx_addr_t newfuture) {
  return;
}

void hpx_lco_newfuture_free_all(hpx_addr_t newfutures) {
  return;
}

bool hpx_lco_newfuture_is_shared() {
  return true;
}
