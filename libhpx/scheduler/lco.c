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
/// @file libhpx/scheduler/lco.c
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "libsync/sync.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "libhpx/parcel.h"
#include "lco.h"

/// We pack state into the LCO pointer---least-significant-bit is already used
/// in the sync_lockable_ptr interface
#define _TRIGGERED_MASK    (0x2)
#define _USER_MASK         (0x4)
#define _STATE_MASK        (0x7)

/// Remote action interface to a future
static hpx_action_t  _lco_wait_action = 0;
static hpx_action_t   _lco_get_action = 0;
static hpx_action_t  _lco_fini_action = 0;
static hpx_action_t _lco_error_action = 0;

hpx_action_t hpx_lco_set_action = 0;

/// return the class pointer, masking out the state.
static const lco_class_t *
_lco_class(lco_t *lco)
{
  const lco_class_t *class = sync_lockable_ptr_read((lockable_ptr_t*)lco);
  uintptr_t bits = (uintptr_t)class;
  bits = bits & ~_STATE_MASK;
  return (lco_class_t*)bits;
}


/// ----------------------------------------------------------------------------
/// Local LCO event handler wrappers.
///
/// These extract the LCO class pointer and forward to the function pointers
/// that it contains.
/// ----------------------------------------------------------------------------
static void
_lco_fini_local(lco_t *lco, hpx_addr_t sync)
{
  _lco_class(lco)->on_fini(lco, sync);
}


static void
_lco_set_local(lco_t *lco, size_t size, const void *data, hpx_addr_t sync)
{
  _lco_class(lco)->on_set(lco, size, data, sync);
}


static void
_lco_error_local(lco_t *lco, uintptr_t code, hpx_addr_t sync)
{
  _lco_class(lco)->on_error(lco, code, sync);
}


static hpx_status_t
_lco_get_local(lco_t *lco, size_t n, void *buffer)
{
  return _lco_class(lco)->on_get(lco, n, buffer);
}


static hpx_status_t
_lco_wait_local(lco_t *lco)
{
  return _lco_class(lco)->on_wait(lco);
}


/// ----------------------------------------------------------------------------
/// Action LCO event handler wrappers.
///
/// These try and pin the LCO, and then forward to the local event handler
/// wrappers. If the pin fails, then the LCO isn't local, so the parcel is
/// resent.
/// ----------------------------------------------------------------------------
static int
_lco_fini(void *args)
{
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco))
    return HPX_RESEND;

  _lco_fini_local(lco, HPX_NULL);
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


static int
_lco_set(void *data)
{
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco))
    return HPX_RESEND;

  // for now, we don't care about local completion because we know that the
  // action interface guarantees that we have exclusive access to the data
  _lco_set_local(lco, hpx_thread_current_args_size(), data, HPX_NULL);
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


static int
_lco_error(uintptr_t *code)
{
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco))
    return HPX_RESEND;

  // for now, we don't care about local completion because we know that the
  // action interface guarantees that we have exclusive access to the data
  _lco_error_local(lco, *code, HPX_NULL);
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


static int
_lco_get(int *n)
{
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco;
  if (!hpx_gas_try_pin(target, (void**)&lco))
    return HPX_RESEND;

  char buffer[*n];                  // ouch---rDMA, or preallocate continuation?
  hpx_status_t status = _lco_get_local(lco, *n, buffer);
  hpx_gas_unpin(target);
  if (status == HPX_SUCCESS)
    hpx_thread_continue(*n, buffer);
  else
    hpx_thread_exit(status);
}


static int
_lco_wait(void *args)
{
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_gas_try_pin(target, (void**)&lco))
    return HPX_RESEND;

  hpx_action_t status = _lco_wait_local(lco);
  hpx_gas_unpin(target);
  hpx_thread_exit(status);
}


/// ----------------------------------------------------------------------------
/// Register the event handlers.
/// ----------------------------------------------------------------------------
static void HPX_CONSTRUCTOR
_initialize_actions(void)
{
  _lco_fini_action   = HPX_REGISTER_ACTION(_lco_fini);
  _lco_error_action  = HPX_REGISTER_ACTION(_lco_error);
  hpx_lco_set_action = HPX_REGISTER_ACTION(_lco_set);
  _lco_get_action    = HPX_REGISTER_ACTION(_lco_get);
  _lco_wait_action   = HPX_REGISTER_ACTION(_lco_wait);
}


/// ----------------------------------------------------------------------------
/// LCO bit packing and manipulation
/// ----------------------------------------------------------------------------
void
lco_init(lco_t *lco, const lco_class_t *class, uintptr_t user)
{
  uintptr_t bits = (uintptr_t)class;
  bits = bits | ((user) ? 0 : _USER_MASK);
  *lco = (const lco_class_t *)bits;
}


void
lco_set_user(lco_t *lco)
{
  uintptr_t bits = (uintptr_t)*lco;
  bits = bits | _USER_MASK;
  *lco = (const lco_class_t *)bits;
}


void
lco_reset_user(lco_t *lco)
{
  uintptr_t bits = (uintptr_t)*lco;
  bits = bits & ~_USER_MASK;
  *lco = (const lco_class_t*)bits;
}


uintptr_t
lco_get_user(const lco_t *lco)
{
  uintptr_t bits = (uintptr_t)*lco;
  return bits & _USER_MASK;
}


void
lco_set_triggered(lco_t *lco)
{
  uintptr_t bits = (uintptr_t)*lco;
  bits = bits | _TRIGGERED_MASK;
  *lco = (const lco_class_t *)bits;
}


void
lco_reset_triggered(lco_t *lco)
{
  uintptr_t bits = (uintptr_t)*lco;
  bits = bits & ~_TRIGGERED_MASK;
  *lco = (const lco_class_t*)bits;
}


uintptr_t
lco_get_triggered(const lco_t *lco)
{
  uintptr_t bits = (uintptr_t)*lco;
  return bits & _TRIGGERED_MASK;
}


void
hpx_lco_delete(hpx_addr_t target, hpx_addr_t sync)
{
  lco_t *lco = NULL;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    _lco_fini_local(lco, sync);
    hpx_gas_unpin(target);
    return;
  }

  hpx_call_async(target, _lco_fini_action, NULL, 0, sync, HPX_NULL);
}


void
hpx_lco_error(hpx_addr_t target, uintptr_t code, hpx_addr_t sync)
{
  lco_t *lco = NULL;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    _lco_error_local(lco, code, sync);
    hpx_gas_unpin(target);
    return;
  }

  hpx_call_async(target, _lco_error_action, &code, sizeof(code), sync,
                 HPX_NULL);
}


void
hpx_lco_set(hpx_addr_t target, const void *value, int size, hpx_addr_t sync)
{
  lco_t *lco = NULL;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    _lco_set_local(lco, size, value, sync);
    hpx_gas_unpin(target);
    return;
  }

  hpx_call_async(target, hpx_lco_set_action, value, size, sync, HPX_NULL);
}


hpx_status_t
hpx_lco_wait(hpx_addr_t target)
{
  lco_t *lco;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    hpx_status_t status = _lco_wait_local(lco);
    hpx_gas_unpin(target);
    return status;
  }

  return hpx_call_sync(target, _lco_wait_action, NULL, 0, NULL, 0);
}


/// ----------------------------------------------------------------------------
/// If the LCO is local, then we use the local get functionality.
/// ----------------------------------------------------------------------------
hpx_status_t
hpx_lco_get(hpx_addr_t target, void *value, int size)
{
  lco_t *lco;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    hpx_status_t status =  _lco_get_local(lco, size, value);
    hpx_gas_unpin(target);
    return status;

  }

  return hpx_call_sync(target, _lco_get_action, &size, sizeof(size),
                       value, size);
}


void
hpx_lco_wait_all(int n, hpx_addr_t lcos[])
{
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
      hpx_call_async(lcos[i], _lco_wait_action, NULL, 0, HPX_NULL, remotes[i]);
    }
  }

  // Wait on all of the lcos sequentially. If the lco is local (i.e., we have a
  // local translation for it) we use the local get operation, otherwise we wait
  // for the completion of the remote proxy.
  for (int i = 0; i < n; ++i) {
    if (locals[i] != NULL) {
      _lco_wait_local(locals[i]);
      hpx_gas_unpin(lcos[i]);
    }
    else {
      hpx_lco_wait(remotes[i]);
      hpx_lco_delete(remotes[i], HPX_NULL);
    }
  }
}


void
hpx_lco_get_all(int n, hpx_addr_t lcos[], void *values[], int sizes[]) {
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
      hpx_call_async(lcos[i], _lco_get_action, &sizes[i], sizeof(sizes[i]),
                     HPX_NULL, remotes[i]);
    }
  }

  // Wait on all of the lcos sequentially. If the lco is local (i.e., we have a
  // local translation for it) we use the local get operation, otherwise we wait
  // for the completion of the remote proxy.
  for (int i = 0; i < n; ++i) {
    if (locals[i] != NULL) {
      _lco_get_local(locals[i], sizes[i], values[i]);
      hpx_gas_unpin(lcos[i]);
    }
    else {
      hpx_lco_get(remotes[i], values[i], sizes[i]);
      hpx_lco_delete(remotes[i], HPX_NULL);
    }
  }
}
