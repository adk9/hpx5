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
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "locality.h"
#include "scheduler.h"
#include "network.h"
#include "lco.h"
#include "thread.h"

/// ----------------------------------------------------------------------------
/// Local future interface.
/// ----------------------------------------------------------------------------
/// @{
typedef struct {
  lco_t lco;                                    // future "is-an" lco
  void *value;
} future_t;

static void _init(future_t *f, int size) {
  bool inplace = (size <= sizeof(f->value));
  lco_init(&f->lco, inplace);
  f->value = (inplace) ? NULL : malloc(size);
  assert(!inplace || !f->value);
}

static void _lock(future_t *f) {
  lco_lock(&f->lco);
}

static void _unlock(future_t *f) {
  lco_unlock(&f->lco);
}

static int _is_inplace(const future_t *f) {
  return lco_is_user(&f->lco);
}

static int _is_set(const future_t *f) {
  return lco_is_set(&f->lco);
}

static void _get_value(const future_t *f, void *out, int size) {
  if (!out || !size)
    return;

  const void *from = (_is_inplace(f)) ? &f->value : f->value;
  memcpy(out, from, size);
}

static void _set_value(future_t *f, const void *from, int size) {
  if (!from || !size)
    return;

  void *to = (_is_inplace(f)) ? &f->value : f->value;
  memcpy(to, from, size);
}

static void _delete(future_t *f) {
  if (!f)
    return;

  // acquire the lock for the future
  _lock(f);
  if (!_is_inplace(f))
    free(f->value);
  free(f);
}
/// @}

/// ----------------------------------------------------------------------------
/// Future actions.
/// ----------------------------------------------------------------------------
/// @{
static hpx_action_t _future_set = 0;
static hpx_action_t _future_get_proxy = 0;
static hpx_action_t _future_delete = 0;

typedef struct {
  int size;
  char data[];
} _future_set_args_t;

static int _future_set_action(void *args) {
  _future_set_args_t *a = args;
  hpx_future_set(scheduler_current_target(), &a->data, a->size);
  hpx_thread_exit(HPX_SUCCESS, NULL, 0);
}

static int _future_get_proxy_action(void *args) {
  int n = *(int*)args;
  char buffer[n];
  hpx_future_get(scheduler_current_target(), buffer, n);
  hpx_thread_exit(HPX_SUCCESS, buffer, n);
}

static int _future_delete_action(void *args) {
  hpx_addr_t target = scheduler_current_target();
  _delete(target.local);
  hpx_thread_exit(HPX_SUCCESS, NULL, 0);
}
/// @}

static void HPX_CONSTRUCTOR _register_actions(void) {
  _future_set = locality_action_register("_future_set", _future_set_action);
  _future_get_proxy = locality_action_register("_future_get_proxy",
                                          _future_get_proxy_action);
  _future_delete = locality_action_register("_future_delete", _future_delete_action);
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
  if (!_is_set(f))
    scheduler_wait(&f->lco);
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
static hpx_addr_t _spawn_get_proxy(hpx_addr_t future, int size) {
  hpx_addr_t cont = hpx_future_new(size);
  hpx_call(future, _future_get_proxy, &size, sizeof(size), cont);
  return cont;
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
    hpx_addr_t val = _spawn_get_proxy(future, size);
    hpx_future_get(val, out, size);
    hpx_future_delete(val);
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
  hpx_addr_t proxies[n];

  // do address translation and spawn all of the remote reads, can't compact
  // this because we need to maintain the mapping from
  // future->values->sizes... actually we could compact them if we wanted to
  // store more information
  for (unsigned i = 0; i < n; ++i) {
    if (network_addr_is_local(futures[i], (void**)&local[i])) {
      proxies[i] = HPX_NULL;
    }
    else {
      local[i] = NULL;
      proxies[i] = _spawn_get_proxy(futures[i], sizes[i]);
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
    hpx_addr_t proxy = proxies[i];
    if (!hpx_addr_eq(proxy, HPX_NULL)) {
      void *addr = (values[i]) ? values[i] : NULL;
      int size = (sizes[i]) ? sizes[i] : 0;
      hpx_future_get(proxy, addr, size);
      hpx_future_delete(proxy);
    }
  }
}

/// If the future is local, set its value and signal it, while holding the
/// lock. Otherwise, send a parcel to its locality to perform a remote future
/// set.
///
/// @todo With rDMA this might be able to put the future value directly using
///       AGAS.
void
hpx_future_set(hpx_addr_t future, const void *value, int size) {
  future_t *f = NULL;
  if (network_addr_is_local(future, (void**)&f)) {
    _lock(f);
    _set_value(f, value, size);
    scheduler_signal(&f->lco);
    _unlock(f);
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
