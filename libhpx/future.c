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
/// @file future.h
/// Defines the future structure.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "parcel.h"
#include "future.h"
#include "thread.h"
#include "scheduler.h"
#include "network.h"
#include "locks.h"

typedef struct {
  int size;
  char data[];
} _future_set_args_t;

static hpx_action_t _future_set = 0;
static hpx_action_t _future_get_proxy = 0;
static hpx_action_t _future_delete = 0;

static int
_future_set_action(void *args) {
  _future_set_args_t *a = args;
  thread_t *t = thread_current();
  hpx_parcel_t *p = t->parcel;
  hpx_future_set(p->target, &a->data, a->size);
  hpx_thread_exit(HPX_SUCCESS, NULL, 0);
}

static int
_future_get_proxy_action(void *args) {
  thread_t *t = thread_current();
  hpx_parcel_t *p = t->parcel;
  int n = *(int*)args;
  char buffer[n];
  hpx_future_get(p->target, buffer, n);
  hpx_thread_exit(HPX_SUCCESS, buffer, n);
}


static void _delete(future_t *f);

static int
_future_delete_action(void *args) {
  thread_t *thread = thread_current();
  hpx_parcel_t *parcel = thread->parcel;
  assert(parcel);
  _delete(parcel->target.local);
  hpx_thread_exit(HPX_SUCCESS, NULL, 0);
}

int
future_init_module(void) {
  _future_set = hpx_action_register("_future_set", _future_set_action);
  _future_get_proxy = hpx_action_register("_future_get_proxy",
                                          _future_get_proxy_action);
  _future_delete = hpx_action_register("_future_delete", _future_delete_action);
  return HPX_SUCCESS;
}

void
future_fini_module(void) {
}

int
future_init_thread(void) {
  return HPX_SUCCESS;
}

void
future_fini_thread(void) {
}

/// ----------------------------------------------------------------------------
/// Futures have three distinct state bits, they can be locked or unlocked, they
/// can be triggered or not, and they can have in place our out of place
/// data. These states are packed into the lowest three bits of the wait queue
/// (which always contains 8-byte aligned pointer values).
/// ----------------------------------------------------------------------------
/// @{
static const uintptr_t _SET        = 0x2;
static const uintptr_t _INPLACE    = 0x4;
/// @}

static bool _is_state(const future_t *f, uintptr_t state) {
  return packed_ptr_is_set(f->waitq, state);
}

static void _set_state(future_t *f, uintptr_t state) {
  packed_ptr_set((void**)&f->waitq, state);
}

static void _lock(future_t *f) {
  packed_ptr_lock((void**)&f->waitq);
}

static void _unlock(future_t *f) {
  packed_ptr_unlock((void**)&f->waitq);
}

static thread_t *_get_queue(future_t *f) {
  return (thread_t*)packed_ptr_get_ptr(f->waitq);
}

/// ----------------------------------------------------------------------------
/// Gets the value, which might be inplace or out of place.
/// ----------------------------------------------------------------------------
static void _get_value(const future_t *f, void *out, int size) {
  if (!out || !size)
    return;

  const void *from = (_is_state(f, _INPLACE)) ? &f->value : f->value;
  memcpy(out, from, size);
}

/// ----------------------------------------------------------------------------
/// Sets the future's value (does not modify state).
/// ----------------------------------------------------------------------------
static void _set_value(future_t *f, const void *from, int size) {
  if (!from || !size)
    return;

  void *to = (_is_state(f, _INPLACE)) ? &f->value : f->value;
  memcpy(to, from, size);
}

/// ----------------------------------------------------------------------------
/// Perform a local get operation.
///
/// Local future blocks caller until the future is set, and then copies its
/// value data into the provided buffer.
///
/// @param      future - the future we're processing
/// @param[out]    out - the output location (may be null)
/// @param        size - the size of the data
/// ----------------------------------------------------------------------------
static void _get_local(future_t *f, void *out, int size) {
  _lock(f);
  if (!_is_state(f, _SET))
    scheduler_wait(f);
  _get_value(f, out, size);
  _unlock(f);
}

/// ----------------------------------------------------------------------------
/// Initiate a remote get operation.
///
/// @param future - the global address of the remote future (may be local)
/// @param   size - the number of bytes we expect back from the get
/// @returns      - the global address of a future to wait on for the completion
/// ----------------------------------------------------------------------------
static hpx_addr_t _spawn_get_remote(hpx_addr_t future, int size) {
  hpx_addr_t cont = hpx_future_new(size);
  hpx_parcel_t *p = hpx_parcel_acquire(sizeof(size));
  hpx_parcel_set_target(p, future);
  hpx_parcel_set_action(p, _future_get_proxy);
  hpx_parcel_set_cont(p, cont);
  memcpy(hpx_parcel_get_data(p), &size, sizeof(size));
  hpx_parcel_send(p);
  return cont;
}

/// ----------------------------------------------------------------------------
/// Finish a remote get operation.
///
/// @param       op - a future representing the remote operation
/// @param[out] out - the local buffer we are getting to
/// @param     size - the amount we are getting
/// ----------------------------------------------------------------------------
static void _sync_get_remote(hpx_addr_t op, void *out, int size) {
  hpx_future_get(op, out, size);
  hpx_future_delete(op);
}

/// ----------------------------------------------------------------------------
/// Get the value of a future.
/// ----------------------------------------------------------------------------
void
hpx_future_get(hpx_addr_t future, void *out, int size) {
  future_t *f = NULL;
  if (network_addr_is_local(future, (void**)&f)) {
    _get_local(f, out, size);
  }
  else {
    hpx_addr_t val = _spawn_get_remote(future, size);
    _sync_get_remote(val, out, size);
  }
}

/// ----------------------------------------------------------------------------
/// Get the value of all of the futures.
/// ----------------------------------------------------------------------------
void
hpx_future_get_all(unsigned n, hpx_addr_t futures[], void *values[],
                   const int sizes[])
{
  // we need to partition the globals into local and remote addresses,
  // uninitialized is fine.
  future_t *local[n];
  hpx_addr_t remote[n];

  // do address translation and spawn all of the remote reads, can't compact
  // this because we need to maintain the mapping from
  // future->values->sizes... actually we could compact them if we wanted to
  // store more information
  for (unsigned i = 0; i < n; ++i) {
    if (network_addr_is_local(futures[i], (void**)&local[i])) {
      remote[i] = HPX_NULL;
    }
    else {
      remote[i] = _spawn_get_remote(futures[i], sizes[i]);
      local[i] = NULL;
    }
  }

  // deal with the local futures sequentially
  for (unsigned i = 0; i < n; ++i) {
    if (local[i] != NULL) {
      void *addr = (values[i]) ? values[i] : NULL;
      int size = (sizes[i]) ? sizes[i] : 0;
      _get_local(local[i], addr, size);
    }
  }

  // deal with the remote futures sequentially
  for (unsigned i = 0; i < n; ++i) {
    if (!hpx_addr_eq(remote[i], HPX_NULL)) {
      void *addr = (values[i]) ? values[i] : NULL;
      int size = (sizes[i]) ? sizes[i] : 0;
      _sync_get_remote(futures[i], addr, size);
    }
  }
}

/// ----------------------------------------------------------------------------
/// ----------------------------------------------------------------------------
void
hpx_future_set(hpx_addr_t future, const void *value, int size) {
  future_t *f = NULL;
  if (network_addr_is_local(future, (void**)&f)) {
    scheduler_signal(f, value, size);
    return;
  }

  hpx_parcel_t *p = hpx_parcel_acquire(sizeof(_future_set_args_t) + size);
  hpx_parcel_set_target(p, future);
  hpx_parcel_set_action(p, _future_set);
  _future_set_args_t *args = hpx_parcel_get_data(p);
  args->size = size;
  memcpy(&args->data, value, size);
  hpx_parcel_send(p);
}


/// ----------------------------------------------------------------------------
/// Use the sync library's least-significant-bit lock to lock the future.
/// ----------------------------------------------------------------------------
void
future_lock(future_t *future) {
  _lock(future);
}

/// ----------------------------------------------------------------------------
/// Signal a future.
///
/// This needs to 1) set the future's value, 2) set the future _SET flag,
/// 3) unlock the future, and 4) return the wait queue to the caller so that
/// waiting threads can be woken up.
///
/// We must acquire the future lock for this operation.
/// ----------------------------------------------------------------------------
thread_t *
future_set(future_t *f, const void *data, int size) {
  _lock(f);
  _set_value(f, data, size);
  _set_state(f, _SET);
  return LOCKABLE_PACKED_STACK_POP_ALL_AND_UNLOCK(&f->waitq);
}

void _delete(future_t *f) {
  if (!f)
    return;

  // acquire the lock for the future
  _lock(f);
  assert(!_get_queue(f));
  if (!_is_state(f, _INPLACE))
    free(f->value);
  free(f);
}

static void _init(future_t *f, int size) {
  f->waitq = NULL;
  if (size > sizeof(f->value))
    f->value = malloc(size);
  else
    _set_state(f, _INPLACE);
}

/// ----------------------------------------------------------------------------
/// Allocate a future.
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_future_new(int size) {
  hpx_addr_t f = network_malloc(sizeof(future_t), sizeof(future_t));
  _init(f.local, size);
  return f;
}

/// ----------------------------------------------------------------------------
/// Free a future.
/// ----------------------------------------------------------------------------
void
hpx_future_delete(hpx_addr_t future) {
  if (future.rank == hpx_get_my_rank())
    _delete(future.local);
  else
    hpx_call(future, _future_delete, NULL, 0, HPX_NULL);
}
