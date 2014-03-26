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
/// @file libhpx/scheduler/counter.c
/// Defines the counter structure.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lco.h"
#include "thread.h"
#include "libhpx/scheduler.h"
#include "libsync/sync.h"

/// ----------------------------------------------------------------------------
/// Local counter interface.
/// ----------------------------------------------------------------------------
/// @{
typedef struct {
  lco_t lco;                                    // counter "is-an" lco
  uint64_t value;                               // the threshold
} counter_t;

/// Initializes the counter LCO.
static void HPX_NON_NULL(1) _counter_init(counter_t *c, uint64_t value) {
  lco_init(&c->lco, 0);
  c->value = value;
}

// Atomically increment the count associated with the counter.
static uint64_t HPX_NON_NULL(1) _counter_dec(counter_t *c, uint64_t amount) {
  int64_t diff = 0 - amount;
  return sync_addf(&c->value, diff, SYNC_RELEASE);
}

// Wrap the counter's lco.
static void HPX_NON_NULL(1) _counter_lock(counter_t *c) {
  lco_lock(&c->lco);
}

static void HPX_NON_NULL(1) _counter_unlock(counter_t *c) {
  lco_unlock(&c->lco);
}

static void HPX_NON_NULL(1) _counter_wait(counter_t *c) {
  scheduler_wait(&c->lco);
}

static void HPX_NON_NULL(1) _counter_signal(counter_t *c) {
  scheduler_signal(&c->lco);
}

/// @}


/// ----------------------------------------------------------------------------
/// Counter actions for remote counter interaction.
/// ----------------------------------------------------------------------------
/// @{
static hpx_action_t _counter_incr_remote = 0;
static hpx_action_t _counter_wait_remote = 0;
static hpx_action_t _counter_delete_remote = 0;

/// Forwards to hpx_lco_counter_delete().
static int _counter_delete_remote_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  hpx_lco_counter_delete(target);
  return HPX_SUCCESS;
}


/// Forwards to hpx_lco_counter_incr().
static int _counter_incr_remote_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  int64_t amount = *(int64_t*)args;
  hpx_lco_counter_incr(target, amount);
  return HPX_SUCCESS;
}


/// Forwards to hpx_lco_counter_wait().
static int _counter_wait_remote_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  hpx_lco_counter_wait(target);
  hpx_thread_exit(HPX_SUCCESS, NULL, 0);
}
/// @}


/// Register the actions that we need to interact with remote counters.
static void HPX_CONSTRUCTOR _register_actions(void) {
  _counter_incr_remote = hpx_register_action("_counter_incr_remote_action",
                                             _counter_incr_remote_action);
  _counter_wait_remote = hpx_register_action("_counter_wait_remote_action",
                                             _counter_wait_remote_action);
  _counter_delete_remote = hpx_register_action("_counter_delete_remote_action",
                                               _counter_delete_remote_action);
}


/// ----------------------------------------------------------------------------
/// Allocate a counter LCO. This is synchronous.
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_lco_counter_new(uint64_t limit) {
  hpx_addr_t counter = hpx_alloc(1, sizeof(counter_t), sizeof(counter_t));
  counter_t *c = NULL;
  int pinned = hpx_addr_try_pin(counter, (void**)&c);
  assert(pinned);
  _counter_init(c, limit);
  hpx_addr_unpin(counter);
  return counter;
}


/// ----------------------------------------------------------------------------
/// Free a counter LCO. This is asynchronous. This does not clean up any waiting
/// threads, in the case where we're freeing before we've reached the
/// limit. Also, it's not properly synchronized with wait or inc.
/// ----------------------------------------------------------------------------
void
hpx_lco_counter_delete(hpx_addr_t counter) {
  counter_t *c = NULL;
  if (hpx_addr_try_pin(counter, (void**)&c)) {
    free(c);
    hpx_addr_unpin(counter);
  }
  else {
    hpx_call(counter, _counter_delete_remote, NULL, 0, HPX_NULL);
  }
}


/// ----------------------------------------------------------------------------
/// Block until the counter LCO's internal count is incremented to its
/// limit. This is synchronous by its nature.
/// ----------------------------------------------------------------------------
void
hpx_lco_counter_wait(hpx_addr_t counter) {
  counter_t *c = NULL;
  if (hpx_addr_try_pin(counter, (void**)&c)) {
    _counter_lock(c);
    if (c->value != 0)
      _counter_wait(c);
    _counter_unlock(c);
    hpx_addr_unpin(counter);
  }
  else {
    hpx_addr_t sync = hpx_future_new(0);
    hpx_call(counter, _counter_wait_remote, NULL, 0, sync);
    hpx_future_get(sync, NULL, 0);
    hpx_future_delete(sync);
  }
}

/// ----------------------------------------------------------------------------
/// Increment the counter. This is asynchronous.
/// ----------------------------------------------------------------------------
void
hpx_lco_counter_incr(hpx_addr_t counter, const uint64_t amount) {
  counter_t *c = NULL;
  if (hpx_addr_try_pin(counter, (void**)&c)) {
    if (_counter_dec(c, amount) == 0) {
      _counter_lock(c);
      _counter_signal(c);
      _counter_unlock(c);
    }
    hpx_addr_unpin(counter);
  }
  else {
    hpx_call(counter, _counter_incr_remote, &amount, sizeof(amount), HPX_NULL);
  }
}
