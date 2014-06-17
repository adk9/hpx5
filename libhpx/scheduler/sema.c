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
/// @file libhpx/scheduler/sema.c
/// @brief Implements the semaphore LCO.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include "hpx/hpx.h"
#include "libhpx/debug.h"
#include "libhpx/scheduler.h"
#include "lco.h"
#include "cvar.h"

/// ----------------------------------------------------------------------------
/// Local semaphore interface.
/// ----------------------------------------------------------------------------
/// @{
typedef struct {
  lco_t     vtable;
  cvar_t available;
  unsigned   count;
} _sema_t;


static hpx_action_t _sema_p_action = 0;
static hpx_action_t _sema_v_action = 0;


// Freelist
static __thread _sema_t *_free_semas = NULL;


static void
_lock(_sema_t *sema)
{
  sync_lockable_ptr_lock((lockable_ptr_t*)&sema->vtable);
}


static void
_unlock(_sema_t *sema)
{
  sync_lockable_ptr_unlock((lockable_ptr_t*)&sema->vtable);
}


static void
_free(_sema_t *sema)
{
  // repurpose vtable as a freelist "next" pointer
  sema->vtable = (lco_class_t*)_free_semas;
  _free_semas = sema;
}


static void
_sema_fini(lco_t *lco, hpx_addr_t sync)
{
  if (!lco)
    return;

  _sema_t *sema = (_sema_t *)lco;
  _lock(sema);
  _free(sema);

  if (!hpx_addr_eq(sync, HPX_NULL))
    hpx_lco_set(sync, NULL, 0, HPX_NULL);
}


static void
_sema_error(lco_t *lco, hpx_status_t code, hpx_addr_t sync)
{
  _sema_t *sema = (_sema_t *)lco;
  _lock(sema);
  scheduler_signal(&sema->available, code);
  _unlock(sema);

  if (!hpx_addr_eq(sync, HPX_NULL))
    hpx_lco_set(sync, NULL, 0, HPX_NULL);
}


/// Set is equivalent to returning a resource to the semaphore.
static void
_sema_set(lco_t *lco, int size, const void *from, hpx_addr_t sync)
{
  _sema_t *sema = (_sema_t *)lco;
  _lock(sema);
  if (sema->count++ == 0) {
    hpx_status_t status = sema->available.status;
    scheduler_signal(&sema->available, status);
  }
  _unlock(sema);
}


/// Get is equivalent to acquiring a resource from the semaphore.
static hpx_status_t
_sema_get(lco_t *lco, int size, void *out) {
  hpx_status_t status = HPX_SUCCESS;
  _sema_t *sema = (_sema_t *)lco;
  _lock(sema);

  // wait until the count is non-zero---MESA semantics mean we re-read count
  unsigned count = sema->count;
  while (count == 0 && status == HPX_SUCCESS) {
    status = scheduler_wait((lockable_ptr_t*)sema, &sema->available);
    count = sema->count;
  }

  // reduce the count
  if (status == HPX_SUCCESS)
    sema->count = count - 1;

  _unlock(sema);
  return status;
}


static hpx_status_t
_sema_wait(lco_t *lco) {
  return _sema_get(lco, 0, NULL);
}


static void
_sema_init(_sema_t *sema, unsigned count)
{
  // the semaphore vtable
  static const lco_class_t vtable = {
    _sema_fini,
    _sema_error,
    _sema_set,
    _sema_get,
    _sema_wait
  };

  lco_init(&sema->vtable, &vtable, 0);
  sema->count = count;
}


static int
_sema_p(void *args)
{
  hpx_addr_t target = hpx_thread_current_target();
  hpx_lco_sema_p(target);
  return HPX_SUCCESS;
}


static int
_sema_v(void *args)
{
  hpx_addr_t target = hpx_thread_current_target();
  hpx_lco_sema_v(target);
  return HPX_SUCCESS;
}


static void HPX_CONSTRUCTOR
_initialize_actions(void)
{
  _sema_p_action = HPX_REGISTER_ACTION(_sema_p);
  _sema_v_action = HPX_REGISTER_ACTION(_sema_v);
}

/// @}


/// ----------------------------------------------------------------------------
/// Allocate a semaphore LCO. This is synchronous.
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_lco_sema_new(unsigned count)
{
  hpx_addr_t sema;
  _sema_t *local = _free_semas;
  if (local) {
    _free_semas = (_sema_t *)local->vtable;
    sema = HPX_HERE;
    char *base;
    if (!hpx_gas_try_pin(sema, (void**)&base)) {
      dbg_error("Could not translate local block.\n");
      hpx_abort();
    }
    hpx_gas_unpin(sema);
    sema.offset = (char*)local - base;
    assert(sema.offset < sema.block_bytes);
  }
  else {
    sema = hpx_gas_alloc(sizeof(_sema_t));
    if (!hpx_gas_try_pin(sema, (void**)&local)) {
      dbg_error("Could not pin newly allocated semaphore.\n");
      hpx_abort();
    }
  }
  _sema_init(local, count);
  hpx_gas_unpin(sema);
  return sema;
}


/// ----------------------------------------------------------------------------
/// Decrement a semaphore.
///
/// If the semaphore is local, then we can use the _sema_get operation directly,
/// otherwise we perform the operation as a synchronous remote call using the
/// _sema_p action.
///
/// @param sema - the global address of the semaphore we're reducing
/// @returns    - HPX_SUCCESS, or an error code if the sema is in an error state
/// ----------------------------------------------------------------------------
hpx_status_t
hpx_lco_sema_p(hpx_addr_t sema)
{
  lco_t *s;
  if (hpx_gas_try_pin(sema, (void**)&s)) {
    hpx_status_t status = _sema_get(s, 0, NULL);
    hpx_gas_unpin(sema);
    return status;
  }

  return hpx_call_sync(sema, _sema_p_action, NULL, 0, NULL, 0);
}


/// ----------------------------------------------------------------------------
/// Increment a semaphore.
///
/// If the semaphore is local, then we can use the _sema_set operation directly,
/// otherwise we perform the operation as an asynchronous remote call using the
/// _sema_v action.
///
/// @param sema - the global address of the semaphore we're incrementing
/// ----------------------------------------------------------------------------
void
hpx_lco_sema_v(hpx_addr_t sema)
{
  lco_t *s;
  if (hpx_gas_try_pin(sema, (void**)&s)) {
    _sema_set(s, 0, NULL, HPX_NULL);
    hpx_gas_unpin(sema);
    return;
  }

  hpx_call_async(sema, _sema_v_action, NULL, 0, HPX_NULL, HPX_NULL);
}
