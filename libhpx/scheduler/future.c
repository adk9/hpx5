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
/// @file libhpx/scheduler/future.c
/// Defines the future structure.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "lco.h"

/// ----------------------------------------------------------------------------
/// Local future interface.
/// ----------------------------------------------------------------------------
/// @{
typedef struct {
  lco_t lco;                                    // future "is-an" lco
  void *value[2];                               // 2 in-place words/future
} _future_t;


/// Freelist allocation for futures.
static __thread _future_t *_free = NULL;


/// Remote block initialization
static hpx_action_t _block_init = 0;
static hpx_action_t _blocks_init = 0;


/// Use the LCO's "user" state to remember if the future is in-place or not.
static int _is_inplace(const _future_t *f) {
  return lco_is_user(&f->lco);
}


/// Deletes the future's out of place data, if necessary.
///
/// NB: deadlock issue here
static void _delete(_future_t *f) {
  if (!f)
    return;

  lco_lock(&f->lco);
  if (!_is_inplace(f)) {
    void *ptr = NULL;
    memcpy(&ptr, &f->value[0], sizeof(ptr));    // strict aliasing
    free(ptr);
  }
  lco_fini(&f->lco);


  // overload the vtable pointer for freelisting---not perfect, but it's
  // reinitialized in _init(), so it's not the end of the world
  f->lco.vtable = (lco_class_t*)_free;
  _free = f;
}


/// Copies @p from into the appropriate location. Futures are single-assignment,
/// so we only do this if the future isn't set yet.
static void _set(_future_t *f, int size, const void *from, hpx_status_t status)
{
  lco_lock(&f->lco);
  if (lco_is_set(&f->lco))
    goto exit;

  if (!from)
    goto signal;

  if (_is_inplace(f)) {
    memcpy(&f->value[0], from, size);
  }
  else {
    void *ptr = NULL;
    memcpy(&ptr, &f->value[0], sizeof(ptr));    // strict aliasing
    memcpy(ptr, from, size);
  }

 signal:
  scheduler_signal(&f->lco, status);

 exit:
  lco_unlock(&f->lco);
}


/// Copies the appropriate value into @p out, waiting if the lco isn't set yet.
static hpx_status_t _get(_future_t *f, int size, void *out) {
  hpx_status_t status;
  lco_lock(&f->lco);
  if (!lco_is_set(&f->lco))
    scheduler_wait(&f->lco);

  // even though future's are single-assignment, we're still going to hold the
  // lock during the memcpy. This is because we want to make sure that the
  // future doesn't get deleted while we're copying.
  if (out == NULL)
    goto exit;

  if (_is_inplace(f)) {
    memcpy(out, &f->value[0], size);
  }
  else {
    void *ptr = NULL;
    memcpy(&ptr, &f->value[0], sizeof(ptr));  // strict aliasing
    memcpy(out, ptr, size);
  }

 exit:
  status = lco_get_status(&f->lco);
  lco_unlock(&f->lco);
  return status;
}


/// The future vtable.
static lco_class_t _vtable = LCO_CLASS_INIT(_delete, _set, _get);


/// initialize the future
static void _init(_future_t *f, int size) {
  bool inplace = (size <= sizeof(f->value));
  lco_init(&f->lco, &_vtable, inplace);
  memset(&f->value, 0, sizeof(f->value));
  if (!inplace) {
    f->value[0] = malloc(size);   // allocate if necessary
    assert(f->value[0]);
  }
}


/// Initialize a block of futures.
static int _block_init_action(uint32_t *args) {
  hpx_addr_t target = hpx_thread_current_target();
  _future_t *futures = NULL;

  // application level forwarding if the future block has moved
  if (!hpx_gas_try_pin(target, (void**)&futures))
    return HPX_RESEND;

  // sequentially initialize each future
  uint32_t size = args[0];
  uint32_t block_size = args[1];
  for (uint32_t i = 0; i < block_size; ++i)
    _init(&futures[i], size);

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


/// Initialize a strided block of futures
static int _blocks_init_action(uint32_t *args) {
  hpx_addr_t base = hpx_thread_current_target();
  uint32_t block_size = args[1];
  uint32_t block_bytes = block_size * sizeof(_future_t);
  uint32_t blocks = args[2];

  hpx_addr_t and = hpx_lco_and_new(blocks);
  for (uint32_t i = 0; i < blocks; i++) {
    hpx_addr_t block = hpx_addr_add(base, i * here->ranks * block_bytes);
    hpx_call(block, _block_init, args, 2 * sizeof(*args), and);
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  return HPX_SUCCESS;
}


static void HPX_CONSTRUCTOR _initialize_actions(void) {
  _block_init  = HPX_REGISTER_ACTION(_block_init_action);
  _blocks_init = HPX_REGISTER_ACTION(_blocks_init_action);
}


/// ----------------------------------------------------------------------------
/// Allocate a future.
///
/// Futures are always allocated in the global address space, because their
/// addresses are used as the targets of parcels.
///
/// @param size - the number of bytes of data the future should be prepared to
///               deal with
/// @returns    - the global address of the allocated future
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_lco_future_new(int size) {
  hpx_addr_t f;
  _future_t *local = _free;
  if (local) {
    _free = (_future_t*)local->lco.vtable;
    f = HPX_HERE;
    char *base;
    if (!hpx_gas_try_pin(f, (void**)&base)) {
      dbg_error("Could not translate local block.\n");
      hpx_abort();
    }
    f.offset = (char*)local - base;
    assert(f.offset < f.block_bytes);
  }
  else {
    f = hpx_gas_alloc(sizeof(_future_t));
    if (!hpx_gas_try_pin(f, (void**)&local)) {
      dbg_error("Could not pin newly allocated future of size %d.\n", size);
      hpx_abort();
    }
  }
  _init(local, size);
  hpx_gas_unpin(f);
  return f;
}


/// ----------------------------------------------------------------------------
/// Allocate a global array of futures.
///
/// Each of the futures needs to be initialized correctly, and if they need to
/// be out of place, then each locality needs to allocate the out-of-place size
/// required.
///
/// @param          n - the (total) number of futures to allocate
/// @param       size - the payload size for the futures
/// @param block_size - the number of futures per block
/// ----------------------------------------------------------------------------
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
    hpx_call(there, _blocks_init, args, sizeof(args), and[0]);
  }

  for (int i = 0; i < rem; ++i) {
    hpx_addr_t block = hpx_addr_add(base, args[2] * here->ranks + i *
                                    block_bytes);
    hpx_call(block, _block_init, args, 2 * sizeof(args[0]), and[1]);

  }
  hpx_lco_wait_all(2, and);
  hpx_lco_delete(and[0], HPX_NULL);
  hpx_lco_delete(and[1], HPX_NULL);

  // return the base address of the allocation
  return base;
}


/// ----------------------------------------------------------------------------
/// Application level programmer doesn't know how big the future is, so we
/// provide this array indexer.
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_lco_future_array_at(hpx_addr_t array, int i) {
  return hpx_addr_add(array, i * sizeof(_future_t));
}


/// ----------------------------------------------------------------------------
/// This should probably be managed by the LCO superclass.
/// ----------------------------------------------------------------------------
void
hpx_lco_future_array_delete(hpx_addr_t array, hpx_addr_t sync) {
  dbg_log("unimplemented");
  if (!hpx_addr_eq(sync, HPX_NULL))
    hpx_lco_set(sync, NULL, 0, HPX_NULL);
}
