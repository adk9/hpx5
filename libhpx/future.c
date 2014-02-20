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

/// ----------------------------------------------------------------------------
/// @file future.h
/// Defines the future structure.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "future.h"
#include "ustack.h"
#include "scheduler.h"
#include "network.h"
#include "sync/locks.h"

/// ----------------------------------------------------------------------------
/// Futures have three distinct state bits, they can be locked or unlocked, they
/// can be triggered or not, and they can have in place our out of place
/// data. These states are packed into the lowest three bits of the wait queue
/// (which always contains 8-byte aligned pointer values).
///
/// These flags read and write the state. Note that _LOCKED must be the
/// least-significant-bit in order to use the lsb_lock/unlock functionality in
/// sync.
/// ----------------------------------------------------------------------------
/// @{
static const uintptr_t _LOCKED     = 0x1;
static const uintptr_t _SET  = 0x2;
static const uintptr_t _INPLACE    = 0x4;
static const uintptr_t _STATE_MASK = 0x7;
/// @}

/// ----------------------------------------------------------------------------
/// Mask out the state so we can read the real wait queue address.
///
/// @prarm future - the future to query
/// @returns      - @p stack, with the state masked out
/// ----------------------------------------------------------------------------
static ustack_t *_get_queue(const future_t *future) {
  return (ustack_t*)((uintptr_t)future->waitq & ~_STATE_MASK);
}

/// ----------------------------------------------------------------------------
/// Gets the composite state from the future.
/// ----------------------------------------------------------------------------
static uintptr_t _get_state(const future_t *future) {
  return (uintptr_t)future->waitq & _STATE_MASK;
}

/// ----------------------------------------------------------------------------
/// Query the state of the future.
///
/// Composite state works fine here, at the cost of an additional equals check.
///
/// @param future - the future to query
/// @returns      - true, if all of the bits in state are set in future->waitq
/// ----------------------------------------------------------------------------
static bool _is_state(const future_t *future, uintptr_t state) {
  return ((uintptr_t)future->waitq & state) == state;
}

/// ----------------------------------------------------------------------------
/// Gets the value, which might be inplace or out of place.
/// ----------------------------------------------------------------------------
static void _get_value(const future_t *f, void *out, int size) {
  if (!out || !size)
    return;

  const void *from = (_is_state(f, _INPLACE)) ? &f->value : f->value;
  memcpy(out, from, size);
}

/// ----------------------------------------------------------------------------
/// Sets the state bits in @p stack to match those in @p future->waitq.
///
/// Should only be called with a "clean" @p stack value, i.e., one with no bits
/// set. We could relax this requirement by masking out the low bits of stack
/// first, but we don't bother.
/// ----------------------------------------------------------------------------
static ustack_t *_set_state_bits(ustack_t *stack, uintptr_t state) {
  return (ustack_t*)((uintptr_t)stack | state);
}


/// ----------------------------------------------------------------------------
/// Sets the future's value (does not modify state).
/// ----------------------------------------------------------------------------
static void _set_value(future_t *f, const void *from, int size) {
  if (!from || !size)
    return;

  void *to = (_is_state(f, _INPLACE)) ? &f->value : f->value;
  memcpy(to, from, size);
}

/// ----------------------------------------------------------------------------
/// Composes the act of unlocking the future, with modifying its queue and
/// state.
///
/// @param future - the future to unlock
/// @param  stack - the new head of the wait queue (should be linked already)
/// @param  state - a state to set
/// ----------------------------------------------------------------------------
static void _unlock(future_t *future, ustack_t *stack, uintptr_t state) {
  // create the new state as the composite of the old state and the new state,
  // but unlocked
  uintptr_t current = _get_state(future);
  current |= state;
  current &= ~_LOCKED;

  // set the state bits in the stack pointer
  stack = _set_state_bits(stack, current);

  // perform the atomic unlock
  lsb_unlock_with_value((void**)&future->waitq, stack);
}

/// ----------------------------------------------------------------------------
/// Perform a local get operation.
///
/// Local future blocks caller until the future is set, and then copies its
/// value data into the provided buffer.
///
/// @param      future - the future we're processing
/// @param[out]    out - the output location (may be null)
/// @param        size - the size of the data
/// ----------------------------------------------------------------------------
static void _get_local(future_t *f, void *out, int size) {
  future_lock(f);
  if (!_is_state(f, _SET))
    scheduler_yield(f);

  assert(_is_state(f, _SET | _LOCKED));
  assert(_get_queue(f) == NULL);
  _get_value(f, out, size);
  _unlock(f, NULL, 0);
  return;
}

/// ----------------------------------------------------------------------------
/// An action that a thread can run to serve as a remote proxy for a get.
/// ----------------------------------------------------------------------------
static hpx_action_t _future_get_proxy = HPX_ACTION_NULL;

/// ----------------------------------------------------------------------------
/// Initiate a remote get operation.
///
/// @param future - the global address of the remote future (may be local)
/// @param   size - the number of bytes we expect back from the get
/// @returns      - the global address of a future to wait on for the completion
/// ----------------------------------------------------------------------------
static hpx_addr_t _spawn_get_remote(hpx_addr_t future, int size) {
  hpx_addr_t cont = hpx_future_new(size);
  hpx_parcel_t *p = hpx_parcel_acquire(0);
  hpx_parcel_set_target(p, future);
  hpx_parcel_set_action(p, _future_get_proxy);
  hpx_parcel_set_cont(p, cont);
  hpx_parcel_send(p);
  return cont;
}

/// ----------------------------------------------------------------------------
/// Finish a remote get operation.
///
/// @param       op - a future representing the remote operation
/// @param[out] out - the local buffer we are getting to
/// @param     size - the amount we are getting
/// ----------------------------------------------------------------------------
static void _sync_get_remote(hpx_addr_t op, void *out, int size) {
  hpx_future_get(op, out, size);
  hpx_future_delete(op);
}

/// ----------------------------------------------------------------------------
/// Allocate a future.
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_future_new(int size) {
  return HPX_NULL;
}

/// ----------------------------------------------------------------------------
/// Free a future.
/// ----------------------------------------------------------------------------
void
hpx_future_delete(hpx_addr_t future) {
}

/// ----------------------------------------------------------------------------
/// Signal a future.
///
/// This needs to 1) set the future's value, 2) set the future _SET flag,
/// 3) unlock the future, and 4) return the wait queue to the caller so that
/// waiting threads can be woken up.
///
/// We must acquire the future lock for this operation.
/// ----------------------------------------------------------------------------
ustack_t *
future_signal(future_t *future, const void *data, int size) {
  ustack_t *waiters = NULL;
  future_lock(future);
  assert(!_is_state(future, _SET));
  _set_value(future, data, size);
  waiters = _get_queue(future);
  _unlock(future, NULL, _SET);
  return waiters;
}

/// ----------------------------------------------------------------------------
/// Get the value of a future.
/// ----------------------------------------------------------------------------
void
hpx_future_get(hpx_addr_t future, void *out, int size) {
  future_t *f = NULL;
  if (network_addr_is_local(future, (void**)&f))
    _get_local(f, out, size);
  else
    _sync_get_remote(_spawn_get_remote(future, size), out, size);
}

/// ----------------------------------------------------------------------------
/// Get the value of all of the futures.
/// ----------------------------------------------------------------------------
void
hpx_future_get_all(unsigned n, hpx_addr_t futures[], void *values[],
                   const int sizes[])
{
  // we need to partition the globals into local and remote addresses,
  // uninitialized is fine.
  future_t *local[n];
  hpx_addr_t remote[n];

  // do address translation and spawn all of the remote reads, can't compact
  // this because we need to maintain the mapping from
  // future->values->sizes... actually we could compact them if we wanted to
  // store more information
  for (unsigned i = 0; i < n; ++i) {
    if (network_addr_is_local(futures[i], (void**)&local[i])) {
      remote[i] = HPX_NULL;
    }
    else {
      remote[i] = _spawn_get_remote(futures[i], sizes[i]);
      local[i] = NULL;
    }
  }

  // deal with the local futures sequentially
  for (unsigned i = 0; i < n; ++i) {
    if (local[i] != NULL) {
      void *addr = (values[i]) ? values[i] : NULL;
      int size = (sizes[i]) ? sizes[i] : 0;
      _get_local(local[i], addr, size);
    }
  }

  // deal with the remote futures sequentially
  for (unsigned i = 0; i < n; ++i) {
    if (remote[i] != HPX_NULL) {
      void *addr = (values[i]) ? values[i] : NULL;
      int size = (sizes[i]) ? sizes[i] : 0;
      _sync_get_remote(futures[i], addr, size);
    }
  }
}

/// ----------------------------------------------------------------------------
/// Use the sync library's least-significant-bit lock to lock the future.
/// ----------------------------------------------------------------------------
void
future_lock(future_t *future) {
  lsb_lock((void**)&future->waitq);
}

/// ----------------------------------------------------------------------------
/// Enqueue stack, and release the lock.
///
/// Must mask out the state for setting the stack->next pointer, and then mask
/// the state back into the stack bits, before using the lsb_unlock_with_value()
/// interface.
/// ----------------------------------------------------------------------------
void
future_wait(future_t *future, ustack_t *stack) {
  stack->next = _get_queue(future);
  _unlock(future, stack, 0);
}
