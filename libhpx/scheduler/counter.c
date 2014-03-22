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
  hpx_addr_t future;                            // set when limit is reached
  uint64_t count;                               // current count
  uint64_t limit;                               // the threshold
} counter_t;

/// Initializes the counter LCO.
static void _init(counter_t *c, uint64_t limit) {
  lco_init(&c->lco, true);
  c->future = hpx_future_new(sizeof(c->count));
  c->count = 0;
  c->limit = limit;
}

/// Deletes the counter.
static void _delete(counter_t *c) {
  if (!c)
    return;

  hpx_future_delete(c->future);
  free(c);
}

// Try to atomically increment the count associated with the counter.
static bool _try_inc_count(counter_t *c, uint64_t count, uint64_t amount) {
  return sync_cas(&c->count, count, count + amount, SYNC_RELEASE, SYNC_RELAXED);
}
/// @}


/// ----------------------------------------------------------------------------
/// Counter actions for remote counter interaction.
/// ----------------------------------------------------------------------------
/// @{
static hpx_action_t _counter_incr = 0;
static hpx_action_t _counter_wait_proxy = 0;
static hpx_action_t _counter_delete = 0;


static int _counter_incr_action(void *args) {
  assert(args != NULL);
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t amount = *(uint64_t*)args;
  hpx_lco_counter_incr(target, amount);
  return HPX_SUCCESS;
}

/// Performs a wait action on behalf of a remote thread, and sets the
/// continuation buffer with the result.
static int _counter_wait_proxy_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t value = hpx_lco_counter_wait(target);
  hpx_thread_exit(HPX_SUCCESS, &value, sizeof(value));
}

/// Deletes a counter by translating it into a local address and calling delete.
static int _counter_delete_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  void *local = NULL;
  if (!hpx_addr_try_pin(target, local))
    hpx_abort(1);

  _delete(local);
  return HPX_SUCCESS;
}
/// @}

static void HPX_CONSTRUCTOR _register_actions(void) {
  _counter_incr = hpx_register_action("_counter_incr", _counter_incr_action);
  _counter_wait_proxy = hpx_register_action("_counter_wait_proxy", _counter_wait_proxy_action);
  _counter_delete = hpx_register_action("_counter_delete", _counter_delete_action);
}

/// ----------------------------------------------------------------------------
/// Initiate a remote wait operation.
///
/// @param future - the global address of the remote counter (may be local)
/// @returns      - the global address of a future to wait on for the completion
/// ----------------------------------------------------------------------------
static hpx_addr_t _spawn_wait_proxy(hpx_addr_t counter) {
  hpx_addr_t cont = hpx_future_new(sizeof(uint64_t));
  hpx_call(counter, _counter_wait_proxy, NULL, 0, cont);
  return cont;
}

/// ----------------------------------------------------------------------------
/// Allocate a counter LCO.
///
/// Malloc enough local space, with the right alignment, and then use the
/// initializer.
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_lco_counter_new(uint64_t limit) {
  hpx_addr_t c = hpx_global_calloc(1, sizeof(counter_t), 1, 8);
  void *local;
  if (!hpx_addr_try_pin(c, &local))
    hpx_abort(1);
  _init(local, limit);
  return c;
}

/// ----------------------------------------------------------------------------
/// Free a counter LCO.
///
/// If the counter LCO is local, go ahead and delete it, otherwise
/// generate a parcel to do it.
/// ----------------------------------------------------------------------------
void
hpx_lco_counter_delete(hpx_addr_t counter) {
  void *local;
  if (hpx_addr_try_pin(counter, &local)) {
    _delete(local);
    hpx_addr_unpin(counter);
  }
  else
    hpx_call(counter, _counter_delete, NULL, 0, HPX_NULL);
}

/// ----------------------------------------------------------------------------
/// Block until the counter LCO's internal count is incremented to its
/// limit. This simply does a future get which handles both, the local
/// and remote cases.
/// ----------------------------------------------------------------------------
uint64_t
hpx_lco_counter_wait(hpx_addr_t counter) {
  counter_t *c = NULL;
  uint64_t value;
  if (hpx_addr_try_pin(counter, (void**)&c)) {
    hpx_future_get(c->future, &value, sizeof(value));
    hpx_addr_unpin(counter);
  } else {
    hpx_addr_t val = _spawn_wait_proxy(counter);
    hpx_future_get(val, &value, sizeof(value));
    hpx_future_delete(val);
  }
  return value;
}

/// ----------------------------------------------------------------------------
/// If the counter LCO is local, increment its value and only signal
/// when the incremented value reaches the limit, while holding the
/// lock. Otherwise, send a parcel to its locality to perform a remote
/// counter increment.
/// ----------------------------------------------------------------------------
void
hpx_lco_counter_incr(hpx_addr_t counter, const uint64_t amount) {
  counter_t *c = NULL;
  uint64_t count;
  if (hpx_addr_try_pin(counter, (void**)&c)) {
    bool success = false;
    do {
      count = sync_load(&c->count, SYNC_ACQUIRE);
      success = _try_inc_count(c, count, amount);
    } while (!success);

    if (count + amount >= c->limit)
      hpx_future_set(c->future, &count, sizeof(count));
    hpx_addr_unpin(counter);
    return;
  }

  hpx_parcel_t *p = hpx_parcel_acquire(sizeof(amount));
  hpx_parcel_set_target(p, counter);
  hpx_parcel_set_action(p, _counter_incr);
  void *args = hpx_parcel_get_data(p);
  memcpy(args, &amount, sizeof(amount));
  hpx_parcel_send(p);
}
