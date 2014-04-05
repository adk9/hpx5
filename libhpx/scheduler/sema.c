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
#include "libhpx/scheduler.h"
#include "lco.h"


/// ----------------------------------------------------------------------------
/// Local semaphore interface.
/// ----------------------------------------------------------------------------
/// @{
typedef struct {
  lco_t lco;
  unsigned count;
} _sema_t;


static void _delete(_sema_t *sema) {
  if (!sema)
    return;

  lco_lock(&sema->lco);
  lco_fini(&sema->lco);
}


/// Get is equivalent to P in a semaphore.
static void _get(_sema_t *sema, int size, void *out) {
  lco_lock(&sema->lco);

  // wait until the count is non-zero
  unsigned count = sema->count;
  while (count == 0) {
    scheduler_wait(&sema->lco);
    count = sema->count;
  }

  // if I'm going to make the count 0, then reset the semaphore so that
  // lco_wait()s will work
  if (count == 1)
    lco_reset(&sema->lco);

  sema->count = count - 1;
  lco_unlock(&sema->lco);
}


/// Get is equivalent to V in the semaphore.
static void _set(_sema_t *sema, int size, const void *from) {
  lco_lock(&sema->lco);
  unsigned count = sema->count++;
  if (count == 0)
    scheduler_signal(&sema->lco);
  lco_unlock(&sema->lco);
}


/// The semaphore vtable.
static lco_class_t _vtable = LCO_CLASS_INIT(_delete, _set, _get);


static void _init(_sema_t *sema, unsigned count) {
  lco_init(&sema->lco, &_vtable, 0);
  sema->count = count;

  // if the count is non-zero, then signal the semaphore to make sure that
  // waiters don't wait---waiting on a semaphore is a bit of an odd operation
  // anyway
  if (count != 0) {
    lco_lock(&sema->lco);
    scheduler_signal(&sema->lco);
    lco_unlock(&sema->lco);
  }
}


static hpx_action_t _p = 0;
static hpx_action_t _v = 0;


static int _p_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  hpx_lco_sema_p(target);
  return HPX_SUCCESS;
}


static int _v_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  hpx_addr_t cont = hpx_thread_current_cont();
  hpx_lco_sema_v(target, cont);
  return HPX_SUCCESS;
}


static void HPX_CONSTRUCTOR _register_actions(void) {
  _p = HPX_REGISTER_ACTION(_p_action);
  _v = HPX_REGISTER_ACTION(_v_action);
}

/// @}


/// ----------------------------------------------------------------------------
/// Allocate a semaphore LCO. This is synchronous.
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_lco_sema_new(unsigned count) {
  hpx_addr_t sema = hpx_alloc(sizeof(_sema_t));
  _sema_t *s = NULL;
  if (!hpx_addr_try_pin(sema, (void**)&s))
    assert(false);
  _init(s, count);
  hpx_addr_unpin(sema);
  return sema;
}


/// ----------------------------------------------------------------------------
/// Decrement a semaphore.
/// ----------------------------------------------------------------------------
void hpx_lco_sema_p(hpx_addr_t sema) {
  _sema_t *s = NULL;
  if (hpx_addr_try_pin(sema, (void**)&s)) {
    _get(s, 0, NULL);
    hpx_addr_unpin(sema);
  }
  else {
    hpx_call(sema, _p, NULL, 0, HPX_NULL);
  }
}


/// ----------------------------------------------------------------------------
/// Increment a semaphore.
/// ----------------------------------------------------------------------------
void hpx_lco_sema_v(hpx_addr_t sema, hpx_addr_t sync) {
  _sema_t *s = NULL;
  if (hpx_addr_try_pin(sema, (void**)&s)) {
    _set(s, 0, NULL);
    if (!hpx_addr_eq(sync, HPX_NULL))
      hpx_lco_set(sync, NULL, 0, HPX_NULL);
    hpx_addr_unpin(sema);
  }
  else {
    hpx_call(sema, _v, NULL, 0, sync);
  }
}
