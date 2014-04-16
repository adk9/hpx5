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
#include "libhpx/scheduler.h"
#include "libhpx/parcel.h"
#include "lco.h"


/// We pack state into the LCO. The least-significant bit is the LOCK bit, the
/// second least significant bit is the SET bit, and the third least significant
/// bit is the USER bit, which can be used by "subclasses" to store subclass
/// specific date, if desired.
///
/// Furthermore, when an LCO has been set, we store an hpx_status_t in the high
/// order 4 bytes of the list pointer.
#define _LOCK_MASK   (0x1)
#define _SET_MASK    (0x2)
#define _USER_MASK   (0x4)
#define _STATE_MASK  (0x7)
#define _STATUS_MASK (0xFFFFFFFF00000000)
#define _QUEUE_MASK  ~_STATE_MASK
#define _UNSET_MASK  ~_SET_MASK
#define _UNLOCK_MASK ~_LOCK_MASK

/// Remote action interface to a future
static hpx_action_t _wait   = 0;
static hpx_action_t _get    = 0;
static hpx_action_t _set    = 0;
static hpx_action_t _delete = 0;


/// Wait on a local LCO. This properly acquires and releases the LCO's lock, and
/// signals the LCO if necessary.
static hpx_status_t _wait_local(lco_t *lco) {
  hpx_status_t status;
  lco_lock(lco);
  if (!lco_is_set(lco))
    scheduler_wait(lco);
  status = lco_get_status(lco);
  lco_unlock(lco);
  return status;
}


typedef struct {
  hpx_status_t status;
  int size;
  char data[];
} _set_args_t;


/// ----------------------------------------------------------------------------
/// Set tries to perform a local set operation. If it fails, it means that the
/// future was moved at some point, so we just resend the parcel, which will
/// ultimately get to the actual LCO and pin it so we can set its value.
/// ----------------------------------------------------------------------------
static int _set_action(_set_args_t *args) {
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_addr_try_pin(target, (void**)&lco))
    return HPX_RESEND;

  lco->vtable->set(lco, args->size, &args->data, args->status); // unpack args
  hpx_addr_unpin(target);
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Get tries to perform a local get operation, and continue the "gotten" value
/// to the continuation address. If the target isn't local then we just resend
/// the parcel, which will ultimately get to the actual LCO and pin it so we can
/// get its value.
/// ----------------------------------------------------------------------------
static int _get_action(int *n) {
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco;
  if (!hpx_addr_try_pin(target, (void**)&lco))
    return HPX_RESEND;

  char buffer[*n];                  // ouch---rDMA, or preallocate continuation?
  hpx_status_t status = lco->vtable->get(lco, *n, buffer);
  hpx_addr_unpin(target);
  if (status == HPX_SUCCESS)
    hpx_thread_continue(*n, buffer);
  else
    hpx_thread_exit(status);
}


/// ----------------------------------------------------------------------------
/// Wait tries to perform a local wait operation. If the target isn't local,
/// then we just resend the parcel, which will ultimately get to the actual LCO
/// and pin it so we can wait.
/// ----------------------------------------------------------------------------
static int _wait_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_addr_try_pin(target, (void**)&lco))
    return HPX_RESEND;

  hpx_action_t status = _wait_local(lco);
  hpx_addr_unpin(target);
  hpx_thread_exit(status);
}


static int _delete_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco = NULL;
  if (!hpx_addr_try_pin(target, (void**)&lco))
    return HPX_RESEND;

  lco->vtable->delete(lco);
  hpx_addr_unpin(target);
  return HPX_SUCCESS;
}


static void HPX_CONSTRUCTOR _initialize_actions(void) {
  _get    = HPX_REGISTER_ACTION(_get_action);
  _set    = HPX_REGISTER_ACTION(_set_action);
  _wait   = HPX_REGISTER_ACTION(_wait_action);
  _delete = HPX_REGISTER_ACTION(_delete_action);
}


void
lco_init(lco_t *lco, const lco_class_t *class, int user) {
  lco->vtable = class;
  uintptr_t bits = (user) ? _USER_MASK : 0;
  lco->queue = (lco_node_t *)bits;
}


void
lco_fini(lco_t *lco) {
  lco_unlock(lco);
}


void
lco_set_user(lco_t *lco) {
  uintptr_t bits = (uintptr_t)lco->queue;
  bits |= _USER_MASK;
  lco->queue = (lco_node_t *)bits;
}


int
lco_is_user(const lco_t *lco) {
  uintptr_t bits = (uintptr_t)lco->queue;
  return bits & _USER_MASK;
}


int
lco_is_set(const lco_t *lco) {
  uintptr_t bits = (uintptr_t)lco->queue;
  return bits & _SET_MASK;
}


hpx_status_t
lco_get_status(const lco_t *lco) {
  uintptr_t bits = (uintptr_t)lco->queue;
  return (hpx_status_t)(bits >> 32);
}


void
lco_reset(lco_t *lco) {
  uintptr_t bits = (uintptr_t)lco->queue;
  bits &= _UNSET_MASK;
  lco->queue = (lco_node_t*)bits;
}


void
lco_lock(lco_t *lco) {
  lco_node_t *from = NULL;

  while (true) {
    // load the queue pointer
    sync_load(from, &lco->queue, SYNC_ACQUIRE);

    // if the lock bit is set, yield and then try again
    uintptr_t bits = (uintptr_t)from;
    if (bits & _LOCK_MASK) {
      scheduler_yield();
      continue;
    }

    // generate a locked "to" value for the packed queue pointer, and try and
    // CAS it into the queue
    lco_node_t *to = (lco_node_t *)(bits | _LOCK_MASK);
    if (!sync_cas(&lco->queue, from, to, SYNC_ACQUIRE, SYNC_RELAXED)) {
      scheduler_yield();
      continue;
    }

    // succeeded
    return;
  }
}


void
lco_unlock(lco_t *lco) {
  // assume we hold the lco, create an unlocked version of the packed queue
  // pointer, and release with the value
  uintptr_t bits = (uintptr_t)lco->queue;
  lco_node_t *to = (lco_node_t *)(bits & _UNLOCK_MASK);
  sync_store(&lco->queue, to, SYNC_RELEASE);
}


lco_node_t *
lco_trigger(lco_t *lco, hpx_status_t status) {
  uintptr_t bits = (uintptr_t)lco->queue;
  lco_node_t *queue = (lco_node_t*)(bits & _QUEUE_MASK); // extract the queue

  assert(sizeof(status) == 4);
  bits &= _STATE_MASK;                          // preserve state
  bits |= _SET_MASK;                            // set the "set" bit
  bits |= (uint64_t)status << 32;               // set the status
  lco_node_t *to = (lco_node_t*)bits;
  sync_store(&lco->queue, to, SYNC_RELAXED);    // update the queue bits

  return queue;                                 // return the queue
}


void
lco_enqueue(lco_t *lco, lco_node_t *node) {
  uintptr_t bits = (uintptr_t)lco->queue;
  uintptr_t state = bits & _STATE_MASK;
  lco_node_t *queue = (lco_node_t*)(bits & _QUEUE_MASK);
  node->next = queue;
  node = (lco_node_t*)((uintptr_t)node | state);
  sync_store(&lco->queue, node, SYNC_RELEASE);
}


void
lco_enqueue_and_unlock(lco_t *lco, lco_node_t *node) {
  uintptr_t bits = (uintptr_t)lco->queue;
  uintptr_t state = bits & _STATE_MASK & _UNLOCK_MASK;
  lco_node_t *queue = (lco_node_t*)(bits & _QUEUE_MASK);
  node->next = queue;
  node = (lco_node_t*)((uintptr_t)node | state);
  sync_store(&lco->queue, node, SYNC_RELEASE);
}


/// ----------------------------------------------------------------------------
/// If the LCO is local, then we use the delete handler. If it's not local, then
/// we forward to a delete proxy.
/// ----------------------------------------------------------------------------
void
hpx_lco_delete(hpx_addr_t target, hpx_addr_t sync) {
  lco_t *lco = NULL;
  if (hpx_addr_try_pin(target, (void**)&lco)) {
    lco->vtable->delete(lco);
    hpx_addr_unpin(target);
    if (!hpx_addr_eq(sync, HPX_NULL))
      hpx_lco_set(sync, NULL, 0, HPX_NULL);
    return;
  }

  hpx_call(target, _delete, NULL, 0, sync);
}


/// ----------------------------------------------------------------------------
/// If the LCO is local, then we use the set aciton handler, If it's not local,
/// then we forward to a set proxy and wait for it to complete.
/// ----------------------------------------------------------------------------
void
hpx_lco_set_status(hpx_addr_t target, const void *value, int size,
                   hpx_status_t status, hpx_addr_t sync) {
  lco_t *lco = NULL;
  if (hpx_addr_try_pin(target, (void**)&lco)) {
    lco->vtable->set(lco, size, value, status);
    hpx_addr_unpin(target);
    if (!hpx_addr_eq(sync, HPX_NULL))
      hpx_lco_set(sync, NULL, 0, HPX_NULL);
  }
  else {
    // We're not local, have to perform set through a proxy. We use a parcel
    // directly here instead of hpx_call, because we want to avoid two
    // serializations of the parcel data---hpx_call requires us to provide an
    // already-serialized argument, while allocating the parcel directly allows
    // us to serialize directly into it. We could provide an HPX call interface
    // that is varargs or something, but that makes the active message
    // instantiation tricky.
    hpx_parcel_t *p = hpx_parcel_acquire(sizeof(_set_args_t) + size);
    assert(p);
    p->target = target;
    p->action = _set;
    p->cont = sync;

    // perform the single serialization
    _set_args_t *args = (_set_args_t *)p->data;
    args->status = HPX_SUCCESS;
    args->size = size;
    memcpy(&args->data, value, size);

    // and send the parcel, waiting for completion
    hpx_parcel_send(p);
  }
}


/// ----------------------------------------------------------------------------
/// If the LCO is local, then we use the set action handler. If it's not local,
/// then we forward to a set proxy and wait for it to complete.
/// ----------------------------------------------------------------------------
void
hpx_lco_set(hpx_addr_t target, const void *value, int size, hpx_addr_t sync) {
  hpx_lco_set_status(target, value, size, HPX_SUCCESS, sync);
}


/// ----------------------------------------------------------------------------
/// If the LCO is local, then we use the local wait functionality. Otherwise, we
/// allocate a local proxy lco to wait on, and wait for the remote target.
/// ----------------------------------------------------------------------------
hpx_status_t
hpx_lco_wait(hpx_addr_t target) {
  hpx_status_t status;
  lco_t *lco;
  if (hpx_addr_try_pin(target, (void**)&lco)) {
    status = _wait_local(lco);
    hpx_addr_unpin(target);
  }
  else {
    hpx_addr_t proxy = hpx_lco_future_new(0);
    hpx_call(target, _wait, NULL, 0, proxy);
    status = hpx_lco_wait(proxy);
    hpx_lco_delete(proxy, HPX_NULL);
  }
  return status;
}


/// ----------------------------------------------------------------------------
/// If the LCO is local, then we use the local get functionality. Otherwise, we
/// allocate a local LCO to wait on, and then initiate the remote operation.
/// ----------------------------------------------------------------------------
hpx_status_t
hpx_lco_get(hpx_addr_t target, void *value, int size) {
  hpx_status_t status;
  lco_t *lco;
  if (hpx_addr_try_pin(target, (void**)&lco)) {
    status = lco->vtable->get(lco, size, value);
    hpx_addr_unpin(target);
  }
  else {
    hpx_addr_t proxy = hpx_lco_future_new(size);
    hpx_call(target, _get, &size, sizeof(size), proxy);
    status = hpx_lco_get(proxy, value, size);
    hpx_lco_delete(proxy, HPX_NULL);
  }
  return status;
}


void
hpx_lco_wait_all(int n, hpx_addr_t lcos[]) {
  // Will partition the lcos up into local and remote LCOs. We waste some stack
  // space here, since, for each lco in lcos, we either have a local mapping or
  // a remote address.
  lco_t *locals[n];
  hpx_addr_t remotes[n];

  // Try and translate (and pin) all of the lcos, for any of the lcos that
  // aren't local, allocate a proxy future and initiate the remote wait. This
  // two-phase approach achieves some parallelism.
  for (int i = 0; i < n; ++i) {
    if (!hpx_addr_try_pin(lcos[i], (void**)&locals[i])) {
      locals[i] = NULL;
      remotes[i] = hpx_lco_future_new(0);
      hpx_call(lcos[i], _wait, NULL, 0, remotes[i]);
    }
  }

  // Wait on all of the lcos sequentially. If the lco is local (i.e., we have a
  // local translation for it) we use the local get operation, otherwise we wait
  // for the completion of the remote proxy.
  for (int i = 0; i < n; ++i) {
    if (locals[i] != NULL) {
      _wait_local(locals[i]);
      hpx_addr_unpin(lcos[i]);
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
    if (!hpx_addr_try_pin(lcos[i], (void**)&locals[i])) {
      locals[i] = NULL;
      remotes[i] = hpx_lco_future_new(sizes[i]);
      hpx_call(lcos[i], _get, &sizes[i], sizeof(sizes[i]), remotes[i]);
    }
  }

  // Wait on all of the lcos sequentially. If the lco is local (i.e., we have a
  // local translation for it) we use the local get operation, otherwise we wait
  // for the completion of the remote proxy.
  for (int i = 0; i < n; ++i) {
    if (locals[i] != NULL) {
      locals[i]->vtable->get(locals[i], sizes[i], values[i]);
      hpx_addr_unpin(lcos[i]);
    }
    else {
      hpx_lco_get(remotes[i], values[i], sizes[i]);
      hpx_lco_delete(remotes[i], HPX_NULL);
    }
  }
}
