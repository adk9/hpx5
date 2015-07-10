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

/// @file libhpx/scheduler/lco.c
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libsync/sync.h>
#include <libhpx/action.h>
#include <libhpx/attach.h>
#include <libhpx/debug.h>
#include <libhpx/instrumentation.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
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
  dbg_assert(lco);
  uintptr_t bits = (uintptr_t)(sync_lockable_ptr_read(&lco->lock));
  bits = bits & ~_STATE_MASK;
  const lco_class_t *class = (lco_class_t*)bits;
  dbg_assert_str(class, "LCO vtable pointer is null, "
                 "this is often an LCO use-after-free\n");
  return class;
}

static hpx_status_t _fini(lco_t *lco) {
  dbg_assert_str(_class(lco)->on_fini, "LCO implementation incomplete\n");
  _class(lco)->on_fini(lco);
  return HPX_SUCCESS;
}

static hpx_status_t _set(lco_t *lco, size_t size, const void *data) {
  const lco_class_t *class = _class(lco);
  dbg_assert_str(class->on_set, "LCO has no on_set handler\n");
  class->on_set(lco, size, data);
  return HPX_SUCCESS;
}

static size_t _size(lco_t *lco) {
  const lco_class_t *class = _class(lco);
  dbg_assert_str(class->on_size, "LCO has no on_size handler\n");
  return class->on_size(lco);
}

static hpx_status_t _error(lco_t *lco, hpx_status_t code) {
  const lco_class_t *class = _class(lco);
  dbg_assert_str(class->on_error, "LCO has no on_error handler\n");
  class->on_error(lco, code);
  return HPX_SUCCESS;
}

static hpx_status_t _reset(lco_t *lco) {
  const lco_class_t *class = _class(lco);
  dbg_assert_str(class->on_reset, "LCO has no on_reset handler\n");
  class->on_reset(lco);
  return HPX_SUCCESS;
}

static hpx_status_t _get(lco_t *lco, size_t bytes, void *out) {
  const lco_class_t *class = _class(lco);
  dbg_assert_str(class->on_get, "LCO has no on_get handler\n");
  return class->on_get(lco, bytes, out);
}

static hpx_status_t _getref(lco_t *lco, size_t bytes, void **out, int *unpin) {
  const lco_class_t *class = _class(lco);
  dbg_assert_str(class->on_getref, "LCO has no on_getref handler\n");
  return class->on_getref(lco, bytes, out, unpin);
}

static hpx_status_t _release(lco_t *lco, void *out) {
  const lco_class_t *class = _class(lco);
  dbg_assert_str(class->on_release, "LCO has no on_release handler\n");
  class->on_release(lco, out);
  return HPX_SUCCESS;
}

static hpx_status_t _wait(lco_t *lco) {
  const lco_class_t *class = _class(lco);
  dbg_assert_str(class->on_wait, "LCO has no on_wait handler\n");
  return class->on_wait(lco);
}

static hpx_status_t _attach(lco_t *lco, hpx_parcel_t *p) {
  const lco_class_t *class = _class(lco);
  dbg_assert_str(class->on_attach, "LCO has no on_attach handler\n");
  return class->on_attach(lco, p);
}

/// Action LCO event handler wrappers.
///
/// These try and pin the LCO, and then forward to the local event handler
/// wrappers. If the pin fails, then the LCO isn't local, so the parcel is
/// resent.
///
/// @{
static int _lco_delete_action_handler(void) {
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    hpx_call_cc(target, hpx_lco_delete_action, NULL, NULL);
  }
  log_lco("deleting lco %p\n", (void*)lco);
  _fini(lco);
  hpx_gas_unpin(target);
  hpx_gas_free(target, HPX_NULL);
  return HPX_SUCCESS;
}
LIBHPX_ACTION(HPX_DEFAULT, 0, hpx_lco_delete_action, _lco_delete_action_handler);

int hpx_lco_set_action_handler(lco_t *lco, void *data, size_t n) {
  return _set(lco, n, data);
}
LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED | HPX_MARSHALLED, hpx_lco_set_action,
           hpx_lco_set_action_handler, HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

static int _lco_error_handler(lco_t *lco, void *args, size_t n) {
  hpx_status_t *code = args;
  return _error(lco, *code);
}
LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED | HPX_MARSHALLED, _lco_error,
           _lco_error_handler, HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

int hpx_lco_reset_action_handler(lco_t *lco) {
  return _reset(lco);
}
LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, hpx_lco_reset_action,
           hpx_lco_reset_action_handler, HPX_POINTER);

static int _lco_size_handler(lco_t *lco, void *UNUSED) {
  return _size(lco);
}
LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _lco_size, _lco_size_handler, HPX_POINTER);

typedef struct {
  lco_t *lco;
  void *buffer;
} _cleanup_release_args_t;

static void _cleanup_release(void *args) {
  _cleanup_release_args_t *a = args;
  _release(a->lco, a->buffer);
}

static int
_lco_get_handler(lco_t *lco, int n) {
  dbg_assert(n > 0);

  // Use the getref handler, no need to worry about pinning here because we
  // _release() in the continuation.
  void *buffer;
  int ignore_unpin;
  hpx_status_t status = _getref(lco, n, &buffer, &ignore_unpin);
  if (status != HPX_SUCCESS) {
    return status;
  }
  _cleanup_release_args_t args = {
    lco,
    buffer
  };
  hpx_thread_continue_cleanup(&_cleanup_release, &args, buffer, n);
}
LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _lco_get, _lco_get_handler, HPX_POINTER, HPX_INT);

static int
_lco_wait_handler(lco_t *lco) {
  return _wait(lco);
}
LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _lco_wait, _lco_wait_handler, HPX_POINTER);

int attach_handler(lco_t *lco, hpx_parcel_t *p, size_t size) {
  hpx_parcel_t *parent = scheduler_current_parcel();
  dbg_assert(hpx_parcel_get_data(parent) == p);
  log_lco("retaining %p, nesting %p\n", (void*)parent, (void*)p);

  parcel_state_t state = parcel_get_state(parent);
  dbg_assert(!parcel_retained(state));
  state |= PARCEL_RETAINED;
  parcel_set_state(parent, state);

  state = parcel_get_state(p);
  dbg_assert(!parcel_nested(state));
  state |= PARCEL_NESTED;

  parcel_set_state(p, state);

#ifdef ENABLE_INSTRUMENTATION
    inst_trace(HPX_INST_CLASS_LCO, HPX_INST_EVENT_LCO_ATTACH_PARCEL, lco,
               hpx_get_my_thread_id(), lco->bits);
#endif

  return _attach(lco, p);
}
LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED | HPX_MARSHALLED, attach,
           attach_handler, HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

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
#ifdef ENABLE_INSTRUMENTATION
    inst_trace(HPX_INST_CLASS_LCO, HPX_INST_EVENT_LCO_INIT, lco,
               hpx_get_my_thread_id(), lco->bits);
#endif
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
#ifdef ENABLE_INSTRUMENTATION
    inst_trace(HPX_INST_CLASS_LCO, HPX_INST_EVENT_LCO_TRIGGER, lco,
               hpx_get_my_thread_id(), lco->bits);
#endif
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
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    int e = hpx_call(target, hpx_lco_delete_action, rsync);
    dbg_check(e, "Could not forward lco_delete\n");
  }
  else {
#ifdef ENABLE_INSTRUMENTATION
    inst_trace(HPX_INST_CLASS_LCO, HPX_INST_EVENT_LCO_DELETE, lco,
               hpx_get_my_thread_id(), lco->bits);
#endif
    log_lco("deleting lco %p\n", (void*)lco);
    int e = _fini(lco);
    hpx_gas_unpin(target);
    hpx_gas_free(target, HPX_NULL);
    hpx_lco_error(rsync, e, HPX_NULL);
  }
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
#ifdef ENABLE_INSTRUMENTATION
    inst_trace(HPX_INST_CLASS_LCO, HPX_INST_EVENT_LCO_RESET, lco,
               hpx_get_my_thread_id(), lco->bits);
#endif
    _reset(lco);
    hpx_gas_unpin(addr);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return;
  }

  int e = hpx_call(addr, hpx_lco_reset_action, rsync);
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
#ifdef ENABLE_INSTRUMENTATION
    inst_trace(HPX_INST_CLASS_LCO, HPX_INST_EVENT_LCO_SET, lco,
               hpx_get_my_thread_id(), lco->bits);
#endif
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
#ifdef ENABLE_INSTRUMENTATION
    inst_trace(HPX_INST_CLASS_LCO, HPX_INST_EVENT_LCO_WAIT, target,
               hpx_get_my_thread_id(), lco->bits);
#endif
    hpx_status_t status = _wait(lco);
    hpx_gas_unpin(target);
    return status;
  }

  return hpx_call_sync(target, _lco_wait, NULL, 0);
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

/// If the LCO is local, then we use the local get functionality. If the LCO
/// isn't local, then we use the network's get functionality.
hpx_status_t hpx_lco_get(hpx_addr_t target, int size, void *value) {
  if (size == 0) {
    return hpx_lco_wait(target);
  }

  dbg_assert(value);
  lco_t *lco;
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    return network_lco_get(here->network, target, size, value);
  }

  hpx_status_t status = _get(lco, size, value);
  hpx_gas_unpin(target);
  return status;
}


// If the LCO isn't local, then we just fall back to the get functionality,
// using a temporary buffer that will be freed at release.
hpx_status_t hpx_lco_getref(hpx_addr_t target, int size, void **out) {
  if (size == 0) {
    return hpx_lco_wait(target);
  }

  dbg_assert(out);
  lco_t *lco;
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    *out = registered_malloc(size);
    dbg_assert(*out);
    return hpx_lco_get(target, size, *out);
  }

  int unpin;
  hpx_status_t e = _getref(lco, size, out, &unpin);
  if (unpin) {
    hpx_gas_unpin(target);
  }
  return e;
}

void hpx_lco_release(hpx_addr_t target, void *out) {
  dbg_assert(out);
  lco_t *lco;
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    // guaranteed to be a registered temporary buffer if the LCO was non-local
    registered_free(out);
  }

  // release tells us if we left the LCO pinned in getref
  int unpin = _release(lco, out);
  if (unpin) {
      hpx_gas_unpin(target);
  }

  // matching unpin for the local pin above
  hpx_gas_unpin(target);
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

int hpx_lco_delete_all(int n, hpx_addr_t *lcos, hpx_addr_t rsync) {
  hpx_addr_t and = HPX_NULL;
  if (rsync) {
    and = hpx_lco_and_new(n);
    int e;
    e = hpx_call_when_with_continuation(and, rsync, hpx_lco_set_action, and, hpx_lco_delete_action, NULL, 0);
    dbg_check(e, "failed to enqueue delete action\n");
  }

  for (int i = 0, e = n; i < e; ++i) {
    if (lcos[i]) {
      hpx_lco_delete(lcos[i], and);
    }
  }

  return HPX_SUCCESS;
}

// Generic array indexer API.
hpx_addr_t hpx_lco_array_at(hpx_addr_t array, int i, int arg) {
  uint32_t lco_bytes = hpx_lco_size(array) + arg;
  return hpx_addr_add(array, i * lco_bytes, UINT32_MAX);
}
