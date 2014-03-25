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
#include <stdlib.h>
#include "hpx/hpx.h"
#include "libhpx/scheduler.h"
#include "lco.h"


/// ----------------------------------------------------------------------------
/// Local semaphore interface.
/// ----------------------------------------------------------------------------
/// @{
typedef struct {
  lco_t lco;
  volatile unsigned count;                      // NB: volatile good enough?
} sema_t;


static void HPX_NON_NULL(1) _sema_init(sema_t *sema, unsigned count) {
  lco_init(&sema->lco, 0);
  sema->count = count;
}


static void HPX_NON_NULL(1) _sema_lock(sema_t *sema) {
  lco_lock(&sema->lco);
}


static void HPX_NON_NULL(1) _sema_unlock(sema_t *sema) {
  lco_unlock(&sema->lco);
}


static void HPX_NON_NULL(1) _sema_wait(sema_t *sema) {
  scheduler_wait(&sema->lco);
}


static void HPX_NON_NULL(1) _sema_signal(sema_t *sema) {
  scheduler_signal(&sema->lco);
}
/// @}


/// ----------------------------------------------------------------------------
/// Remote actions for the HPX semaphore interface.
/// ----------------------------------------------------------------------------
/// @{
static hpx_action_t _sema_p_remote = 0;
static hpx_action_t _sema_v_remote = 0;
static hpx_action_t _sema_delete_remote = 0;


static int _sema_p_remote_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  hpx_sema_p(target);
  return HPX_SUCCESS;
}


static int _sema_v_remote_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  hpx_sema_v(target);
  return HPX_SUCCESS;
}


static int _sema_delete_remote_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  hpx_sema_delete(target);
  return HPX_SUCCESS;
}


static void HPX_CONSTRUCTOR _register_actions(void) {
  _sema_p_remote = hpx_register_action("_sema_p_remote_action",
                                       _sema_p_remote_action);
  _sema_v_remote = hpx_register_action("_sema_v_remote_action",
                                       _sema_v_remote_action);
  _sema_delete_remote = hpx_register_action("_sema_delete_remote_action",
                                            _sema_delete_remote_action);
}


/// ----------------------------------------------------------------------------
/// Allocate a semaphore LCO. This is synchronous.
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_sema_new(unsigned count) {
  hpx_addr_t sema = hpx_global_calloc(1, sizeof(sema_t), 1, sizeof(sema_t));
  sema_t *s = NULL;
  int pinned = hpx_addr_try_pin(sema, (void**)&s);
  assert(pinned);
  _sema_init(s, count);
  hpx_addr_unpin(sema);
  return sema;
}


void hpx_sema_delete(hpx_addr_t sema) {
  sema_t *s = NULL;
  if (hpx_addr_try_pin(sema, (void**)&s)) {
    free(s);
    hpx_addr_unpin(sema);
  }
  else {
    hpx_call(sema, _sema_delete_remote, NULL, 0, HPX_NULL);
  }
}


/// ----------------------------------------------------------------------------
/// Decrement a semaphore. Must block if the semaphore is at 0.
/// ----------------------------------------------------------------------------
void hpx_sema_p(hpx_addr_t sema) {
  sema_t *s = NULL;
  if (!hpx_addr_try_pin(sema, (void**)&s)) {
    hpx_addr_t f = hpx_future_new(0);
    hpx_call(sema, _sema_p_remote, NULL, 0, f);
    hpx_future_get(f, NULL, 0);
    hpx_future_delete(f);
    return;
  }

  _sema_lock(s);
  unsigned count = s->count;
  while (count == 0) {
    _sema_wait(s);
    count = s->count;
  }
  s->count = count - 1;
  _sema_unlock(s);

  hpx_addr_unpin(sema);
}


/// ----------------------------------------------------------------------------
/// Increment a semaphore. Signals the semaphore if the transition is from 0 to
/// 1.
/// ----------------------------------------------------------------------------
void hpx_sema_v(hpx_addr_t sema) {
  sema_t *s = NULL;
  if (!hpx_addr_try_pin(sema, (void**)&s)) {
    hpx_call(sema, _sema_v_remote, NULL, 0, HPX_NULL);
    return;
  }

  _sema_lock(s);
  unsigned count = s->count;
  s->count = count + 1;
  if (count == 0)
    _sema_signal(s);
  _sema_unlock(s);
  hpx_addr_unpin(sema);
}
