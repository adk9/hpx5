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

// ==============================================================
//
// New futures implementation
//
// ==============================================================

typedef struct {
  lco_t lco;
  cvar_t full;
  cvar_t empty;
  uint32_t bits;
  void *value;
} _newfuture_t;

enum  _future_wait_action { _future_wait_full, _future_wait_for, _future_wait_until };

struct _future_wait_args {
  hpx_set_t set;
  enum _future_wait_action wait_action;
  hpx_time_t time;
};

/// Freelist allocation for futures.
static __thread _newfuture_t *_free_futures = NULL;


/// Remote block initialization
static hpx_action_t _future_block_init = 0;
static hpx_action_t _future_blocks_init = 0;
static hpx_action_t _future_reset_remote = 0;
static hpx_action_t _future_wait_remote = 0;

static bool _empty(const _newfuture_t *f) {
  return f->bits & HPX_UNSET;
}

static bool _full(const _newfuture_t *f) {
  return f->bits & HPX_SET;
}

/// Use the LCO's "user" state to remember if the future is in-place or not.
static uintptr_t
_is_inplace(const _newfuture_t *f) {
  return lco_get_user(&f->lco);
}


static hpx_status_t
_wait(_newfuture_t *f) {
  if (!_full(f))
    return scheduler_wait(&f->lco.lock, &f->full);
  else
    return cvar_get_error(&f->full);
}

static void
_free(_newfuture_t *f) {
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

  _newfuture_t *f = (_newfuture_t *)lco;
  lco_lock(&f->lco);

  if (!_is_inplace(f)) {
    void *ptr = NULL;
    memcpy(&ptr, &f->value, sizeof(ptr));       // strict aliasing
    free(ptr);
  }

  _free(f);
}

static void
_future_set(lco_t *lco, int size, const void *from)
{
  _newfuture_t *f = (_newfuture_t *)lco;
  lco_lock(&f->lco);

  hpx_status_t status;
  if (!_empty(f))
    scheduler_wait(&f->lco.lock, &f->empty);
  else {
    status = cvar_get_error(&f->empty);
    if (status != HPX_SUCCESS)
      goto unlock;
  }

  if (from && _is_inplace(f)) {
    memcpy(&f->value, from, size);
  }
  else if (from) {
    void *ptr = NULL;
    memcpy(&ptr, &f->value, sizeof(ptr));       // strict aliasing
    memcpy(ptr, from, size);
  }

  uint64_t buffer;
  if (_is_inplace(f))
    memcpy(&buffer, &f->value, 4);
  else
    memcpy(&buffer, f->value, 4);

  f->bits ^= HPX_UNSET; // not empty anymore!
  f->bits |= HPX_SET;
  cvar_reset(&f->empty);
  scheduler_signal_all(&f->full);
 unlock:
  lco_unlock(&f->lco);
}

static void
_future_reset(lco_t *lco)
{
  _newfuture_t *f = (_newfuture_t *)lco;
  lco_lock(&f->lco);

  cvar_reset(&f->full);
  scheduler_signal_all(&f->empty);

  lco_unlock(&f->lco);
}

static int
_future_reset_remote_action(void *data)
{
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco))
    return HPX_RESEND;
  _future_reset(lco);

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

static void
_future_error(lco_t *lco, hpx_status_t code)
{
  _newfuture_t *f = (_newfuture_t *)lco;
  lco_lock(&f->lco);
  scheduler_signal_error(&f->full, code);
  lco_unlock(&f->lco);
}


/// Copies the appropriate value into @p out, waiting if the lco isn't set yet.
static hpx_status_t
_future_get(lco_t *lco, int size, void *out)
{
  _newfuture_t *f = (_newfuture_t *)lco;
  lco_lock(&f->lco);

  hpx_status_t status = _wait(f);

  if (!_full(f))
    scheduler_wait(&f->lco.lock, &f->full);

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

  f->bits ^= HPX_SET;
  f->bits |= HPX_UNSET;
  cvar_reset(&f->full);
  scheduler_signal_all(&f->empty);

 unlock:
  lco_unlock(&f->lco);
  return status;
}

static hpx_status_t
_future_wait(lco_t *lco)
{
  _newfuture_t *f = (_newfuture_t *)lco;
  lco_lock(&f->lco);
  hpx_status_t status = _wait(f);
  lco_unlock(&f->lco);
  return status;
}

static hpx_status_t
_future_wait_remote_action(struct _future_wait_args *args)
{
  hpx_status_t status;
  hpx_addr_t target = hpx_thread_current_target();
  _newfuture_t *f;
  if (!hpx_gas_try_pin(target, (void**)&f))
    return HPX_RESEND;

  lco_lock(&f->lco);
  
  if (args->set == HPX_SET) {
    if (!_full(f))
      status = scheduler_wait(&f->lco.lock, &f->full);
    else
      status = cvar_get_error(&f->full);
    f->bits ^= HPX_SET;
    f->bits |= HPX_UNSET;
    cvar_reset(&f->full);
    scheduler_signal_all(&f->empty);
  }
  else {
    if (!_empty(f))
      status = scheduler_wait(&f->lco.lock, &f->empty);
    else
      status = cvar_get_error(&f->empty);
    f->bits ^= HPX_UNSET;
    f->bits |= HPX_SET;
    cvar_reset(&f->empty);
    scheduler_signal_all(&f->full);
  }

  lco_unlock(&f->lco);

  hpx_gas_unpin(target);

  return status;
}

/// initialize the future
static void
_future_init(_newfuture_t *f, int size)
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
  cvar_reset(&f->empty);
  cvar_reset(&f->full);
  f->bits = 0 | HPX_UNSET; // future starts out empty
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
  _newfuture_t *futures = NULL;

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
  uint32_t block_bytes = block_size * sizeof(_newfuture_t);
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
  // these actions are for old futures - do we need them still?
  _future_block_init  = HPX_REGISTER_ACTION(_future_block_init_action);
  _future_blocks_init = HPX_REGISTER_ACTION(_future_blocks_init_action);

  // new
  _future_reset_remote = HPX_REGISTER_ACTION(_future_reset_remote_action);
  _future_wait_remote = HPX_REGISTER_ACTION(_future_wait_remote_action);
}


hpx_addr_t
hpx_lco_newfuture_new(size_t size) {
  hpx_addr_t f;
  _newfuture_t *local = _free_futures;
  if (local) {
    _free_futures = (_newfuture_t*)local->value;
    f = HPX_HERE;
    char *base;
    if (!hpx_gas_try_pin(f, (void**)&base)) {
      dbg_error("future: could not translate local block.\n");
    }
    f.offset = (char*)local - base;
    assert(f.offset < f.block_bytes);
  }
  else {
    f = locality_malloc(sizeof(_newfuture_t));
    if (!hpx_gas_try_pin(f, (void**)&local)) {
      dbg_error("future: could not pin newly allocated future of size %llu.\n", (unsigned long long)size);
      hpx_abort();
    }
  }
  _future_init(local, size);
  hpx_gas_unpin(f);
  return f;
}


hpx_addr_t
hpx_lco_newfuture_new_all(int n, size_t size) {
  // perform the global allocation
  // if we want to be consistent with old futures, we'd need to have a 
  // parameter for block_size. Just in case, we keep block_size but set
  // it to 1.
  uint32_t block_size = 1;
  uint32_t blocks = (n / block_size) + ((n % block_size) ? 1 : 0);
  uint32_t block_bytes = block_size * sizeof(_newfuture_t);
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

hpx_addr_t hpx_lco_newfuture_shared_new(size_t size) {
  return hpx_lco_newfuture_new(size);
}

hpx_addr_t hpx_lco_newfuture_shared_new_all(int num_participants, size_t size) {
  return hpx_lco_newfuture_new_all(num_participants, size);
}


// Application level programmer doesn't know how big the future is, so we
// provide this array indexer.
hpx_addr_t
hpx_lco_newfuture_at(hpx_addr_t array, int i) {
  return hpx_addr_add(array, i * sizeof(_newfuture_t));
}

void hpx_lco_newfuture_setat(hpx_addr_t future,  int id, size_t size, void *data,
				     hpx_addr_t lsync_lco, hpx_addr_t rsync_lco) {
  hpx_addr_t future_i = hpx_lco_newfuture_at(future, id);
  hpx_lco_set(future_i, size, data, lsync_lco, rsync_lco);
}

void hpx_lco_newfuture_emptyat(hpx_addr_t base, int i, hpx_addr_t rsync_lco) {
  hpx_addr_t target = hpx_lco_newfuture_at(base, i);
  
  // this is based on the implementation of hpx_lco_set()
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco))
  {
    hpx_call_async(target, _future_reset_remote, NULL, 0, HPX_NULL, rsync_lco);
    return;
  }

  _future_reset(lco);
  hpx_gas_unpin(target);

  if (!hpx_addr_eq(rsync_lco, HPX_NULL))
    hpx_lco_set(rsync_lco, 0, NULL, HPX_NULL, HPX_NULL);
}

hpx_status_t hpx_lco_newfuture_getat(hpx_addr_t base, int i, size_t size, void *value) {
  hpx_addr_t target = hpx_lco_newfuture_at(base, i);
  return hpx_lco_get(target, size, value);
}



// this is a highly suboptimal implementation
// ideally this would be done more like wait_all is implemented
void hpx_lco_newfuture_get_all(size_t num, hpx_addr_t futures, size_t size,
			       void *values[]) {
  hpx_addr_t *lcos = malloc(sizeof(hpx_addr_t) * num);
  int *sizes = malloc(sizeof(int) * num);
  hpx_status_t *statuses = malloc(sizeof(hpx_status_t) * num);

  for (int i = 0; i < num; i++) {
    lcos[i] = hpx_lco_newfuture_at(futures, i);
    sizes[i] = size;
  }

  hpx_lco_get_all(num, lcos, sizes, values, statuses);

  free(lcos);
  free(sizes);
  free(statuses);
}

void hpx_lco_newfuture_waitat(hpx_addr_t future, int id, hpx_set_t set) {
  hpx_addr_t target = hpx_lco_newfuture_at(future, id);

  struct _future_wait_args args;
  args.set = set;
  args.wait_action = _future_wait_full; // TODO remove??

  hpx_call_sync(target, _future_wait_remote, &args, sizeof(args), NULL, 0);
}

hpx_status_t hpx_lco_newfuture_waitat_for(hpx_addr_t future, int id, hpx_set_t set, hpx_time_t time) {
  hpx_time_t abs_time = hpx_time_point(hpx_time_now(), time);

  hpx_addr_t done = hpx_lco_future_new(0);
  struct _future_wait_args *args = malloc(sizeof(args));
  args->set = set;
  args->wait_action = _future_wait_full; // TODO remove??

  hpx_addr_t target = hpx_lco_newfuture_at(future, id);
  hpx_call_async(target, _future_wait_remote, args, sizeof(args), HPX_NULL, done);

  hpx_status_t success = hpx_lco_try_wait(done, abs_time);
  hpx_lco_delete(done, HPX_NULL);
  free(args);

  return success;
}

hpx_status_t hpx_lco_newfuture_waitat_until(hpx_addr_t future, int id, hpx_set_t set, hpx_time_t time) {
  hpx_addr_t done = hpx_lco_future_new(0);
  struct _future_wait_args *args = malloc(sizeof(args));
  args->set = set;
  args->wait_action = _future_wait_full; // TODO remove??

  hpx_addr_t target = hpx_lco_newfuture_at(future, id);
  hpx_call_async(target, _future_wait_remote, args, sizeof(args), HPX_NULL, done);

  hpx_status_t success = hpx_lco_try_wait(done, time);
  hpx_lco_delete(done, HPX_NULL);
  free(args);

  return success;
}

void hpx_lco_newfuture_wait_all(size_t num, hpx_addr_t newfutures, hpx_set_t set) {

  hpx_addr_t done = hpx_lco_and_new(num);
  struct _future_wait_args *args = malloc(sizeof(args));
  args->set = set;
  args->wait_action = _future_wait_full; // TODO remove??

  for (int i = 0; i < num; i++) {
    hpx_addr_t target = hpx_lco_newfuture_at(newfutures, i);
    hpx_call_async(target, _future_wait_remote, args, sizeof(args), HPX_NULL, done);
  }

  hpx_lco_wait(done);
  free(args);

  return;
}

hpx_status_t hpx_lco_newfuture_wait_all_for(size_t num, hpx_addr_t newfutures, 
					    hpx_set_t set, hpx_time_t time) {
  hpx_time_t abs_time = hpx_time_point(hpx_time_now(), time);

  hpx_addr_t done = hpx_lco_and_new(num);
  struct _future_wait_args *args = malloc(sizeof(args));
  args->set = set;
  args->wait_action = _future_wait_full; // TODO remove??

  for (int i = 0; i < num; i++) {
    hpx_addr_t target = hpx_lco_newfuture_at(newfutures, i);
    hpx_call_async(target, _future_wait_remote, args, sizeof(args), HPX_NULL, done);
  }

  hpx_status_t success = hpx_lco_try_wait(done, abs_time);
  hpx_lco_delete(done, HPX_NULL);
  free(args);

  return success;
}

hpx_status_t hpx_lco_newfuture_wait_all_until(size_t num, hpx_addr_t newfutures, 
					    hpx_set_t set, hpx_time_t time) {
  hpx_addr_t done = hpx_lco_and_new(num);
  struct _future_wait_args *args = malloc(sizeof(args));
  args->set = set;
  args->wait_action = _future_wait_full; // TODO remove??

  for (int i = 0; i < num; i++) {
    hpx_addr_t target = hpx_lco_newfuture_at(newfutures, i);
    hpx_call_async(target, _future_wait_remote, args, sizeof(args), HPX_NULL, done);
  }

  hpx_status_t success = hpx_lco_try_wait(done, time);
  hpx_lco_delete(done, HPX_NULL);
  free(args);

  return success;
}



void hpx_lco_newfuture_free(hpx_addr_t newfuture) {



  /* hpx_addr_t f; */
  /* _newfuture_t *local = _free_futures; */
  /* if (local) { */
  /*   _free_futures = (_newfuture_t*)local->value; */
  /*   f = HPX_HERE; */
  /*   char *base; */
  /*   if (!hpx_gas_try_pin(f, (void**)&base)) { */
  /*     dbg_error("future: could not translate local block.\n"); */
  /*   } */
  /*   f.offset = (char*)local - base; */
  /*   assert(f.offset < f.block_bytes); */
  /* } */
  /* else { */
  /*   f = locality_malloc(sizeof(_newfuture_t)); */
  /*   if (!hpx_gas_try_pin(f, (void**)&local)) { */
  /*     dbg_error("future: could not pin newly allocated future of size %llu.\n", (unsigned long long)size); */
  /*     hpx_abort(); */
  /*   } */
  /* } */
  /* _future_init(local, size); */
  /* hpx_gas_unpin(f); */
  /* return f; */


}

void hpx_lco_newfuture_free_all(hpx_addr_t newfutures) {

}
