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

#include "libhpx/scheduler.h"
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
  f->value = (inplace) ? NULL : malloc(size);   // allocate if necessary
  assert(!inplace || !f->value);
}

/// Wrap the LCO's interface into a slightly nicer one
/// @{
static void _lock(future_t *f)            { lco_lock(&f->lco); }
static void _unlock(future_t *f)          { lco_unlock(&f->lco); }
static int _is_inplace(const future_t *f) { return lco_is_user(&f->lco); }
static int _is_set(const future_t *f)     { return lco_is_set(&f->lco); }
/// @}

/// Copies the appropriate value into @p out
static void _get_value(const future_t *f, void *out, int size) {
  if (!out || !size)
    return;

  const void *from = (_is_inplace(f)) ? &f->value : f->value;
  memcpy(out, from, size);
}

/// Copies @p from into the appropriate location
static void _set_value(future_t *f, const void *from, int size) {
  if (!from || !size)
    return;

  void *to = (_is_inplace(f)) ? &f->value : f->value;
  memcpy(to, from, size);
}

/// Deletes the future and it's out of place data, if necessary. Grabs the lock.
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
/// Future actions for remote future interaction.
/// ----------------------------------------------------------------------------
/// @{
static hpx_action_t _future_set = 0;
static hpx_action_t _future_get_proxy = 0;
static hpx_action_t _future_delete = 0;

/// Encapsulates hpx_future_set in an action interface
typedef struct {
  int size;
  char data[];
} _future_set_args_t;

static int _future_set_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  _future_set_args_t *a = args;
  hpx_future_set(target, &a->data, a->size);
  return HPX_SUCCESS;
}


/// Performs a get action on behalf of a remote thread, and sets the
/// continuation buffer with the result.
static int _future_get_proxy_action(void *args) {
  int n = *(int*)args;
  char buffer[n];
  hpx_addr_t target = hpx_thread_current_target();
  hpx_future_get(target, buffer, n);
  hpx_thread_exit(HPX_SUCCESS, buffer, n);
}


/// Deletes a future by translating it into a local address and calling delete.
static int _future_delete_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  void *local = NULL;
  if (!hpx_addr_try_pin(target, local))
    hpx_abort(1);

  _delete(local);
  return HPX_SUCCESS;
}
/// @}

static void HPX_CONSTRUCTOR _register_actions(void) {
  _future_set = hpx_register_action("_future_set", _future_set_action);
  _future_get_proxy = hpx_register_action("_future_get_proxy", _future_get_proxy_action);
  _future_delete = hpx_register_action("_future_delete", _future_delete_action);
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
  if (hpx_addr_try_pin(future, (void**)&f)) {
    _get_local(f, out, size);
    hpx_addr_unpin(future);
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
    if (hpx_addr_try_pin(futures[i], (void**)&local[i])) {
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
      hpx_addr_unpin(futures[i]);
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
  if (hpx_addr_try_pin(future, (void**)&f)) {
    _lock(f);
    _set_value(f, value, size);
    scheduler_signal(&f->lco);
    _unlock(f);
    hpx_addr_unpin(future);
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
///
/// Malloc enough local space, with the right alignment, and then use the
/// initializer.
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_future_new(int size) {
  hpx_addr_t f = hpx_global_calloc(1, sizeof(future_t), 1, sizeof(future_t));
  void *local;
  if (!hpx_addr_try_pin(f, &local))
    hpx_abort(1);
  _init(local, size);
  return f;
}

/// ----------------------------------------------------------------------------
/// Free a future.
///
/// If the future is local, go ahead and delete it, otherwise generate a parcel
/// to do it.
/// ----------------------------------------------------------------------------
void
hpx_future_delete(hpx_addr_t future) {
  void *local;
  if (hpx_addr_try_pin(future, &local)) {
    _delete(local);
    hpx_addr_unpin(future);
  }
  else
    hpx_call(future, _future_delete, NULL, 0, HPX_NULL);
}
