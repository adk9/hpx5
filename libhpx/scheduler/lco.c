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

/// @file libhpx/scheduler/lco.c
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <libsync/sync.h>
#include "libhpx/action.h"
#include "libhpx/attach.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "libhpx/parcel.h"
#include "lco.h"

/// We pack state into the LCO pointer---least-significant-bit is already used
/// in the sync_lockable_ptr interface
#define _TRIGGERED_MASK    (0x2)
#define _DELETED_MASK      (0x4)
#define _STATE_MASK        (0x7)

/// return the class pointer, masking out the state.
static const lco_class_t *_lco_class(lco_t *lco) {
  const lco_class_t *class = sync_lockable_ptr_read(&lco->lock);
  uintptr_t bits = (uintptr_t)class;
  bits = bits & ~_STATE_MASK;
  return (lco_class_t*)bits;
}

/// Action LCO event handler wrappers.
///
/// These try and pin the LCO, and then forward to the local event handler
/// wrappers. If the pin fails, then the LCO isn't local, so the parcel is
/// resent.
HPX_ACTION(_lco_fini, void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    return HPX_RESEND;
  }

  const lco_class_t *class = _lco_class(lco);
  DEBUG_IF(!class->on_fini) {
    return dbg_error("on_fini uninitialized");
  }
  class->on_fini(lco);
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

HPX_ACTION(hpx_lco_set_action, void *data) {
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    return HPX_RESEND;
  }

  size_t size = hpx_thread_current_args_size();

  const lco_class_t *class = _lco_class(lco);
  DEBUG_IF(!class->on_set) {
    return dbg_error("on_set uninitialized");
  }
  class->on_set(lco, size, data);
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

HPX_ACTION(_lco_error, void *args) {
  hpx_status_t *code = args;
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    return HPX_RESEND;
  }

  // for now, we don't care about local completion because we know that the
  // action interface guarantees that we have exclusive access to the data
  const lco_class_t *class = _lco_class(lco);
  DEBUG_IF(!class->on_error) {
    return dbg_error("on_error uninitialized");
  }
  class->on_error(lco, *code);
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

HPX_ACTION(_lco_get, void *args) {
  int *n = args;
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    return HPX_RESEND;
  }

  char buffer[*n];                  // ouch---rDMA, or preallocate continuation?
  const lco_class_t *class = _lco_class(lco);
  DEBUG_IF(!class->on_get) {
    return dbg_error("on_get uninitialized");
  }
  hpx_status_t status = class->on_get(lco, *n, buffer);
  hpx_gas_unpin(target);
  if (status == HPX_SUCCESS) {
    hpx_thread_continue(*n, buffer);
  }
  else {
    return status;
  }
}

HPX_ACTION(_lco_wait, void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    return HPX_RESEND;
  }

  const lco_class_t *class = _lco_class(lco);
  DEBUG_IF(!class->on_wait) {
    return dbg_error("on_wait uninitialized");
  }
  hpx_status_t status = class->on_wait(lco);
  hpx_gas_unpin(target);
  hpx_thread_exit(status);
}

HPX_ACTION(attach, void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    return HPX_RESEND;
  }

  /// @todo: This parcel copy shouldn't be necessary. If we can retain the
  ///        parent parcel and free it appropriately, then we could just enqueue
  ///        the args directly.
  const hpx_parcel_t *p = args;
  hpx_parcel_t *parcel = hpx_parcel_acquire(NULL, p->size);
  assert(parcel_size(p) == parcel_size(parcel));

  DEBUG_IF(parcel == NULL) {
    return dbg_error("could not allocate a parcel to attach\n");
  }
  memcpy(parcel, p, parcel_size(p));

  const lco_class_t *class = _lco_class(lco);
  DEBUG_IF(!class->on_attach) {
    return dbg_error("on_attach uninitialized");
  }
  hpx_status_t status = class->on_attach(lco, parcel);
  hpx_gas_unpin(target);
  return status;
}

/// LCO bit packing and manipulation
/// @{
const lco_class_t *lco_lock(lco_t *lco) {
  const lco_class_t *class = _lco_class(lco);
  DEBUG_IF((uintptr_t)class & _DELETED_MASK) {
    dbg_error("locking lco that was previously deleted");
  }
  return sync_lockable_ptr_lock(&lco->lock);
}

void lco_unlock(lco_t *lco) {
  sync_lockable_ptr_unlock(&lco->lock);
}

void lco_init(lco_t *lco, const lco_class_t *class, uintptr_t deleted) {
  lco->vtable = class;
  lco->bits |= ((deleted) ? _DELETED_MASK : 0);
}

void lco_set_deleted(lco_t *lco) {
  DEBUG_IF(true) {
    lco->bits |= _DELETED_MASK;
  }
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
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    hpx_call_async(target, _lco_fini, HPX_NULL, rsync, NULL, 0);
    return;
  }

  const lco_class_t *class = _lco_class(lco);
  DEBUG_IF(!class->on_fini) {
    dbg_error("on_fini uninitialized");
  }
  class->on_fini(lco);
  hpx_gas_unpin(target);
  hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
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
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    hpx_call_async(target, _lco_error, HPX_NULL, rsync,
                   &code, sizeof(code));
    return;
  }

  const lco_class_t *class = _lco_class(lco);
  DEBUG_IF(!class->on_error) {
    dbg_error("on_error uninitialized");
  }
  class->on_error(lco, code);
  hpx_gas_unpin(target);
  hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
}

void hpx_lco_set(hpx_addr_t target, int size, const void *value,
                 hpx_addr_t lsync, hpx_addr_t rsync)
{
  if (target == HPX_NULL) {
    return;
  }

  lco_t *lco = NULL;
  if ((size > HPX_LCO_SET_ASYNC) || !hpx_gas_try_pin(target, (void**)&lco)) {
    hpx_call_async(target, hpx_lco_set_action, lsync, rsync, value, size);
    return;
  }

  const lco_class_t *class = _lco_class(lco);
  DEBUG_IF(!class->on_set) {
    dbg_error("on_set uninitialized");
  }
  class->on_set(lco, size, value);
  hpx_gas_unpin(target);
  hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
  hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
}


hpx_status_t hpx_lco_wait(hpx_addr_t target) {
  lco_t *lco;
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    return hpx_call_sync(target, _lco_wait, NULL, 0, NULL, 0);
  }

  const lco_class_t *class = _lco_class(lco);
  DEBUG_IF(!class->on_wait) {
    return dbg_error("on_wait uninitialized");
  }
  hpx_status_t status = class->on_wait(lco);
  hpx_gas_unpin(target);
  return status;
}

hpx_status_t hpx_lco_try_wait(hpx_addr_t target, hpx_time_t time) {
  lco_t *lco;
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    hpx_addr_t done = hpx_lco_future_new(0);
    // for optimization purposes we may want to send a time limit,
    // in which case we would also need an lco_try_wait_action
    // but for now, we will just have the remote side wait and we will
    // bail early if necessary
    hpx_status_t status = hpx_call(target, _lco_wait, done, NULL, 0);
    if (status != HPX_SUCCESS) {
      return status;
    }

    return hpx_lco_try_wait(done, time);
  }

  const lco_class_t *class = _lco_class(lco);
  DEBUG_IF(!class->on_try_wait) {
    return dbg_error("on_try_wait uninitialized");
  }
  hpx_status_t status = class->on_try_wait(lco, time);
  hpx_gas_unpin(target);
  return status;
}

/// If the LCO is local, then we use the local get functionality.
hpx_status_t hpx_lco_get(hpx_addr_t target, int size, void *value) {
  lco_t *lco;
  if (!hpx_gas_try_pin(target, (void**)&lco)) {
    return hpx_call_sync(target, _lco_get, value, size, &size, sizeof(size));
  }

  const lco_class_t *class = _lco_class(lco);
  DEBUG_IF(!class->on_get) {
    return dbg_error("on_get  uninitialized");
  }
  hpx_status_t status = class->on_get(lco, size, value);
  hpx_gas_unpin(target);
  return status;
}

int hpx_lco_wait_all(int n, hpx_addr_t lcos[], hpx_status_t statuses[]) {
  // Will partition the lcos up into local and remote LCOs. We waste some stack
  // space here, since, for each lco in lcos, we either have a local mapping or
  // a remote address.
  lco_t *locals[n];
  hpx_addr_t remotes[n];

  // Try and translate (and pin) all of the lcos, for any of the lcos that
  // aren't local, allocate a proxy future and initiate the remote wait. This
  // two-phase approach achieves some parallelism.
  for (int i = 0; i < n; ++i) {
    if (!hpx_gas_try_pin(lcos[i], (void**)&locals[i])) {
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
      const lco_class_t *lco = _lco_class(locals[i]);
      DEBUG_IF(!lco->on_wait) {
        return dbg_error("on_wait uninitialized");
      }
      status = lco->on_wait(locals[i]);
      hpx_gas_unpin(lcos[i]);
    }
    else {
      status = hpx_lco_wait(remotes[i]);
      hpx_lco_delete(remotes[i], HPX_NULL);
    }
    if (status != HPX_SUCCESS) {
      ++errors;
    }
    if (statuses) {
      statuses[i] = status;
    }
  }
  return errors;
}

int hpx_lco_get_all(int n, hpx_addr_t lcos[], int sizes[], void *values[],
                    hpx_status_t statuses[])
{
  // Will partition the lcos up into local and remote LCOs. We waste some stack
  // space here, since, for each lco in lcos, we either have a local mapping or
  // a remote address.
  lco_t *locals[n];
  hpx_addr_t remotes[n];

  // Try and translate (and pin) all of the lcos, for any of the lcos that
  // aren't local, allocate a proxy future and initiate the remote get. This
  // two-phase approach achieves some parallelism.
  for (int i = 0; i < n; ++i) {
    if (!hpx_gas_try_pin(lcos[i], (void**)&locals[i])) {
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
      const lco_class_t *lco = _lco_class(locals[i]);
      DEBUG_IF(!lco->on_get) {
        return dbg_error("on_get uninitialized");
      }
      status = lco->on_get(locals[i], sizes[i], values[i]);
      hpx_gas_unpin(lcos[i]);
    }
    else {
      status = hpx_lco_get(remotes[i], sizes[i], values[i]);
      hpx_lco_delete(remotes[i], HPX_NULL);
    }
    if (status != HPX_SUCCESS) {
      ++errors;
    }
    if (statuses) {
      statuses[i] = status;
    }
  }
  return errors;
}
