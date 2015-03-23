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

/// @file libhpx/scheduler/lco.c
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <libsync/sync.h>
#include <libhpx/action.h>
#include <libhpx/attach.h>
#include <libhpx/locality.h>
#include <libhpx/scheduler.h>
#include <libhpx/parcel.h>
#include "lco.h"
#include "thread.h"

/// We pack state into the LCO pointer---least-significant-bit is already used
/// in the sync_lockable_ptr interface
#define _TRIGGERED_MASK    (0x2)
#define _DELETED_MASK      (0x4)
#define _STATE_MASK        (0x7)

/// return the class pointer, masking out the state.
static const lco_class_t *_class(lco_t *lco) {
  const lco_class_t *class = sync_lockable_ptr_read(&lco->lock);
  uintptr_t bits = (uintptr_t)class;
  bits = bits & ~_STATE_MASK;
  return (lco_class_t*)bits;
}

static hpx_status_t _fini(lco_t *lco) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_fini, "LCO implementation incomplete\n");
  _class(lco)->on_fini(lco);
  return HPX_SUCCESS;
}

static hpx_status_t _set(lco_t *lco, size_t size, const void *data) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_set, "LCO implementation incomplete\n");
  _class(lco)->on_set(lco, size, data);
  return HPX_SUCCESS;
}

static size_t _size(lco_t *lco) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_size, "LCO implementation incomplete\n");
  return _class(lco)->on_size(lco);
}

static hpx_status_t _error(lco_t *lco, hpx_status_t code) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_error, "LCO implementation incomplete\n");
  _class(lco)->on_error(lco, code);
  return HPX_SUCCESS;
}

static hpx_status_t _reset(lco_t *lco) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_reset, "LCO implementation incomplete\n");
  _class(lco)->on_reset(lco);
  return HPX_SUCCESS;
}

static hpx_status_t _get(lco_t *lco, size_t bytes, void *out) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_get, "LCO implementation incomplete\n");
  return _class(lco)->on_get(lco, bytes, out);
}

static hpx_status_t _getref(lco_t *lco, size_t bytes, void **out) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_getref, "LCO implementation incomplete\n");
  return _class(lco)->on_getref(lco, bytes, out);
}

static hpx_status_t _release(lco_t *lco, void *out) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_release, "LCO implementation incomplete\n");
  _class(lco)->on_release(lco, out);
  return HPX_SUCCESS;
}

static hpx_status_t _wait(lco_t *lco) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_wait, "LCO implementation incomplete\n");
  return _class(lco)->on_wait(lco);
}

static hpx_status_t _attach(lco_t *lco, hpx_parcel_t *p) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_attach, "LCO implementation incomplete\n");
  return _class(lco)->on_attach(lco, p);
}

/// Action LCO event handler wrappers.
///
/// These try and pin the LCO, and then forward to the local event handler
/// wrappers. If the pin fails, then the LCO isn't local, so the parcel is
/// resent.
///
/// @{
HPX_PINNED(hpx_lco_delete_action, lco_t *lco, void *args) {
  return _fini(lco);
}

HPX_PINNED(hpx_lco_set_action, lco_t *lco, void *data) {
  return _set(lco, hpx_thread_current_args_size(), data);
}

static HPX_PINNED(_lco_error, lco_t *lco, void *args) {
  hpx_status_t *code = args;
  return _error(lco, *code);
}

HPX_PINNED(hpx_lco_reset_action, lco_t *lco, void *UNUSED) {
  return _reset(lco);
}

static HPX_PINNED(_lco_size, lco_t *lco, void *UNUSED) {
  return _size(lco);
}

static HPX_PINNED(_lco_get, lco_t *lco, void *args) {
  dbg_assert(args);
  int *n = args;
  // convert to wait if there's no buffer
  if (*n == 0) {
    return _wait(lco);
  }

  // otherwise do the get to a stack location and continue it---can
  // get rid of this with a lco_getref()
  char buffer[*n];
  hpx_status_t status = _get(lco, *n, buffer);
  if (status == HPX_SUCCESS) {
    hpx_thread_continue(*n, buffer);
  }
  else {
    return status;
  }
}

static HPX_PINNED(_lco_getref, lco_t *lco, int *n) {
  dbg_assert(n);
  // convert to wait if there's no buffer
  if (*n == 0) {
    return _wait(lco);
  }

  // otherwise continue the LCO buffer
  void *buffer;
  hpx_status_t status = _getref(lco, *n, &buffer);
  if (status == HPX_SUCCESS) {
    hpx_thread_continue(*n, buffer);
  }
  else {
    return status;
  }
}

static HPX_PINNED(_lco_getref_reply, void **local, void *data) {
  size_t bytes = hpx_thread_current_args_size();
  dbg_assert(bytes);
  memcpy(*local, data, bytes);
  return HPX_SUCCESS;
}

static HPX_PINNED(_lco_wait, lco_t *lco, void *args) {
  return _wait(lco);
}

HPX_PINNED(attach, lco_t *lco, hpx_parcel_t *p) {
  hpx_parcel_t *parent = scheduler_current_parcel();
  dbg_assert(hpx_parcel_get_data(parent) == p);
  log("retaining %p, nesting %p\n", parent, p);

  parcel_state_t state = parcel_get_state(parent);
  dbg_assert(!state.retain);
  state.retain = 1;
  parcel_set_state(parent, state);

  state = parcel_get_state(p);
  dbg_assert(!state.nested);
  state.nested = 1;
  parcel_set_state(p, state);

  return _attach(lco, p);
}
/// @}

/// LCO bit packing and manipulation
/// @{
void lco_lock(lco_t *lco) {
  dbg_assert(lco);
  sync_lockable_ptr_lock(&lco->lock);
  dbg_assert(self && self->current);
  struct ustack *stack = parcel_get_stack(self->current);
  dbg_assert(stack);
  stack->lco_depth++;
  log_lco("%p acquired lco %p (in lco class %p)\n", (void*)self->current, (void*)lco,
          (void*)_class(lco));
}

void lco_unlock(lco_t *lco) {
  dbg_assert(lco);
  dbg_assert(self && self->current);
  struct ustack *stack = parcel_get_stack(self->current);
  log_lco("%p released lco %p\n", (void*)self->current, (void*)lco);
  dbg_assert(stack);
  int depth = stack->lco_depth--;
  dbg_assert_str(depth > 0, "mismatched lco acquire release (lco %p)\n",
                 (void*)lco);
  sync_lockable_ptr_unlock(&lco->lock);
}

void lco_init(lco_t *lco, const lco_class_t *class) {
  lco->vtable = class;
}

void lco_fini(lco_t *lco) {
  DEBUG_IF(true) {
    lco->bits |= _DELETED_MASK;
  }
  lco_unlock(lco);
}

void lco_reset_deleted(lco_t *lco) {
  lco->bits &= ~_DELETED_MASK;
}

uintptr_t lco_get_deleted(const lco_t *lco) {
  return lco->bits & _DELETED_MASK;
}

void lco_set_triggered(lco_t *lco) {
  lco->bits |= _TRIGGERED_MASK;
}

void lco_reset_triggered(lco_t *lco) {
  lco->bits &= ~_TRIGGERED_MASK;
}

uintptr_t lco_get_triggered(const lco_t *lco) {
  return lco->bits & _TRIGGERED_MASK;
}

/// @}

void hpx_lco_delete(hpx_addr_t target, hpx_addr_t rsync) {
  lco_t *lco = NULL;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    log_lco("deleting lco %p\n", (void*)lco);
    _fini(lco);
    hpx_gas_unpin(target);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return;
  }

  int e = hpx_call_async(target, hpx_lco_delete_action, HPX_NULL, rsync, NULL, 0);
  dbg_check(e, "Could not forward lco_delete\n");
}

void hpx_lco_error(hpx_addr_t target, hpx_status_t code, hpx_addr_t rsync) {
  if (code == HPX_SUCCESS) {
    hpx_lco_set(target, 0, NULL, HPX_NULL, rsync);
    return;
  }

  if (target == HPX_NULL) {
    return;
  }

  lco_t *lco = NULL;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    _error(lco, code);
    hpx_gas_unpin(target);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return;
  }

  size_t size = sizeof(code);
  int e = hpx_call_async(target, _lco_error, HPX_NULL, rsync, &code, size);
  dbg_check(e, "Could not forward lco_error\n");
}

void hpx_lco_error_sync(hpx_addr_t addr, hpx_status_t code) {
  hpx_addr_t sync = hpx_lco_future_new(0);
  hpx_lco_error(addr, code, sync);
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);
}

void hpx_lco_reset(hpx_addr_t addr, hpx_addr_t rsync) {
  if (addr == HPX_NULL) {
    return;
  }

  lco_t *lco = NULL;
  if (hpx_gas_try_pin(addr, (void**)&lco)) {
    _reset(lco);
    hpx_gas_unpin(addr);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return;
  }

  int e = hpx_call(addr, hpx_lco_reset_action, rsync, NULL, 0);
  dbg_check(e, "Could not forward lco_reset\n");
}

void hpx_lco_reset_sync(hpx_addr_t addr) {
  hpx_addr_t sync = hpx_lco_future_new(0);
  hpx_lco_reset(addr, sync);
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);
}

void hpx_lco_set(hpx_addr_t target, int size, const void *value,
                 hpx_addr_t lsync, hpx_addr_t rsync) {
  if (target == HPX_NULL) {
    return;
  }

  lco_t *lco = NULL;
  if ((size < HPX_LCO_SET_ASYNC) && hpx_gas_try_pin(target, (void**)&lco)) {
    _set(lco, size, value);
    hpx_gas_unpin(target);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return;
  }

  int e = hpx_call_async(target, hpx_lco_set_action, lsync, rsync, value, size);
  dbg_check(e, "Could not forward lco_set\n");
}

hpx_status_t hpx_lco_wait(hpx_addr_t target) {
  lco_t *lco;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    hpx_status_t status = _wait(lco);
    hpx_gas_unpin(target);
    return status;
  }

  return hpx_call_sync(target, _lco_wait, NULL, 0, NULL, 0);
}

size_t hpx_lco_size(hpx_addr_t target) {
  lco_t *lco;
  size_t size = 0;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    size = _size(lco);
    hpx_gas_unpin(target);
    return size;
  }
  return hpx_call_sync(target, _lco_size, &size, size, NULL, 0);
}

/// If the LCO is local, then we use the local get functionality.
hpx_status_t hpx_lco_get(hpx_addr_t target, int size, void *value) {
  lco_t *lco;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    dbg_assert(!size || value);
    dbg_assert(!value || size);
    hpx_status_t status = (size) ? _get(lco, size, value) : _wait(lco);
    hpx_gas_unpin(target);
    return status;
  }
  return hpx_call_sync(target, _lco_get, value, size, &size, sizeof(size));
}

hpx_status_t hpx_lco_getref(hpx_addr_t target, int size, void **out) {
  dbg_assert(out && *out);
  lco_t *lco;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    dbg_assert(!size || out);
    dbg_assert(!out || size);
    hpx_status_t status = (size) ? _getref(lco, size, out) : _wait(lco);
    return status;
  }

  void *buffer = malloc(size);
  assert(buffer);
  hpx_addr_t result = hpx_lco_future_new(sizeof(buffer));
  bool pinned = hpx_gas_try_pin(result, NULL);
  dbg_assert_str(pinned, "failed to pin the local buffer future in hpx_lco_getref.\n");

  hpx_lco_set(result, sizeof(buffer), &buffer, HPX_NULL, HPX_NULL);
  int e = hpx_call_with_continuation(target, _lco_getref, result, _lco_getref_reply,
                                     &size, sizeof(size));
  if (e == HPX_SUCCESS) {
    e = hpx_lco_wait(result);
    *out = buffer;
  }

  hpx_gas_unpin(result);
  hpx_lco_delete(result, HPX_NULL);
  return e;
}

void hpx_lco_release(hpx_addr_t target, void *out) {
  lco_t *lco;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    if (!_release(lco, out)) {
      // unpin the LCO only if it was the original local LCO that we
      // pinned previously.
      hpx_gas_unpin(target);
    }
    hpx_gas_unpin(target);
  } else {
    // if the LCO is not local, delete the copied buffer.
    if (out) {
      free(out);
    }
  }
}

int hpx_lco_wait_all(int n, hpx_addr_t lcos[], hpx_status_t statuses[]) {
  dbg_assert(n > 0);

  // Will partition the lcos up into local and remote LCOs. We waste some
  // space here, since, for each lco in lcos, we either have a local mapping or
  // a remote address. We don't use the stack because we can't control how big
  // @p n gets.
  lco_t **locals = calloc(n, sizeof(*locals));
  dbg_assert_str(locals, "failed to allocate array for %d elements", n);
  hpx_addr_t *remotes = calloc(n, sizeof(*remotes));
  dbg_assert_str(remotes, "failed to allocate array for %d elements", n);

  // Try and translate (and pin) all of the lcos, for any of the lcos that
  // aren't local, allocate a proxy future and initiate the remote wait. This
  // two-phase approach achieves some parallelism.
  for (int i = 0; i < n; ++i) {
    // We neither issue a remote proxy for HPX_NULL, nor wait locally on
    // HPX_NULL. We manually set the status output for these elements to
    // indicate success.
    if (lcos[i] == HPX_NULL) {
      locals[i] = NULL;
      remotes[i] = HPX_NULL;
    }
    else if (hpx_gas_try_pin(lcos[i], (void**)&locals[i])) {
      remotes[i] = HPX_NULL;
    }
    else {
      locals[i] = NULL;
      remotes[i] = hpx_lco_future_new(0);
      hpx_call_async(lcos[i], _lco_wait, HPX_NULL, remotes[i], NULL, 0);
    }
  }

  // Wait on all of the lcos sequentially. If the lco is local (i.e., we have a
  // local translation for it) we use the local get operation, otherwise we wait
  // for the completion of the remote proxy.
  int errors = 0;
  for (int i = 0; i < n; ++i) {
    hpx_status_t status = HPX_SUCCESS;
    if (locals[i] != NULL) {
      status = _wait(locals[i]);
      hpx_gas_unpin(lcos[i]);
    }
    else if (remotes[i] != HPX_NULL) {
      status = hpx_lco_wait(remotes[i]);
      hpx_lco_delete(remotes[i], HPX_NULL);
    }
    else {
      status = HPX_SUCCESS;
    }

    if (status != HPX_SUCCESS) {
      ++errors;
    }

    if (statuses) {
      statuses[i] = status;
    }
  }

  free(remotes);
  free(locals);
  return errors;
}

int hpx_lco_get_all(int n, hpx_addr_t lcos[], int sizes[], void *values[],
                    hpx_status_t statuses[]) {
  dbg_assert(n > 0);

  // Will partition the lcos up into local and remote LCOs. We waste some
  // space here, since, for each lco in lcos, we either have a local mapping or
  // a remote address. We don't use the stack because we can't control how big
  // @p n gets.
  lco_t **locals = calloc(n, sizeof(*locals));
  dbg_assert_str(locals, "failed to allocate array for %d elements", n);
  hpx_addr_t *remotes = calloc(n, sizeof(*remotes));
  dbg_assert_str(remotes, "failed to allocate array for %d elements", n);

  // Try and translate (and pin) all of the lcos, for any of the lcos that
  // aren't local, allocate a proxy future and initiate the remote get. This
  // two-phase approach achieves some parallelism.
  for (int i = 0; i < n; ++i) {
    if (lcos[i] == HPX_NULL) {
      locals[i] = NULL;
      remotes[i] = HPX_NULL;
    }
    else if (hpx_gas_try_pin(lcos[i], (void**)&locals[i])) {
      remotes[i] = HPX_NULL;
    }
    else {
      locals[i] = NULL;
      remotes[i] = hpx_lco_future_new(sizes[i]);
      hpx_call_async(lcos[i], _lco_get, HPX_NULL, remotes[i], &sizes[i],
                     sizeof(sizes[i]));
    }
  }

  // Wait on all of the lcos sequentially. If the lco is local (i.e., we have a
  // local translation for it) we use the local get operation, otherwise we wait
  // for the completion of the remote proxy.
  int errors = 0;
  for (int i = 0; i < n; ++i) {
    hpx_status_t status = HPX_SUCCESS;
    if (locals[i] != NULL) {
      status = _get(locals[i], sizes[i], values[i]);
      hpx_gas_unpin(lcos[i]);
    }
    else if (remotes[i] != HPX_NULL) {
      status = hpx_lco_get(remotes[i], sizes[i], values[i]);
      hpx_lco_delete(remotes[i], HPX_NULL);
    }
    else {
      status = HPX_SUCCESS;
    }

    if (status != HPX_SUCCESS) {
      ++errors;
    }

    if (statuses) {
      statuses[i] = status;
    }
  }

  free(remotes);
  free(locals);
  return errors;
}

// Generic array indexer API.
hpx_addr_t hpx_lco_array_at(hpx_addr_t array, int i, int arg) {
  uint32_t lco_bytes = hpx_lco_size(array) + arg;
  return hpx_addr_add(array, i * lco_bytes, UINT32_MAX);
}
