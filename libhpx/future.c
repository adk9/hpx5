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
static const uintptr_t _TRIGGERED  = 0x2;
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
/// Query the state of the future.
///
/// Composite state works fine here, at the cost of an additional equals check.
///
/// @param future - the future to query
/// @returns      - true, if all of the bits in state are set in future->waitq
/// ----------------------------------------------------------------------------
static bool _is_substate(const future_t *future, uintptr_t state) {
  return ((uintptr_t)future->waitq & state) == state;
}

static void _set_value(future_t *f, const void *from, int size) {
  void *to = (_is_substate(f, _INPLACE)) ? &f->value : f->value;
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
/// Use the sync library's least-significant-bit lock to lock the future.
/// ----------------------------------------------------------------------------
void
future_lock(future_t *future) {
  lsb_lock((void**)&future->waitq);
}

/// ----------------------------------------------------------------------------
/// Use the sync library's least-significant-bit lock to unlock the future.
/// ----------------------------------------------------------------------------
void
future_unlock(future_t *future) {
  lsb_unlock((void**)&future->waitq);
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
  // link stack into the queue
  stack->next = _get_queue(future);

  // unlock the future while updating its queue
  _unlock(future, stack, 0);
}

/// ----------------------------------------------------------------------------
/// ----------------------------------------------------------------------------
const void*
future_get_value(const future_t *f, void *out, int size) {
  if (!out)
    return f->value;

  const void *from = (_is_substate(f, _INPLACE)) ? &f->value : f->value;
  memcpy(out, from, size);
  return f->value;
}

/// ----------------------------------------------------------------------------
/// Signal a future.
///
/// This needs to 1) set the future's value, 2) set the future _TRIGGERED flag,
/// 3) unlock the future, and 4) return the wait queue to the caller so that
/// waiting threads can be woken up.
///
/// We must acquire the future lock for this operation.
/// ----------------------------------------------------------------------------
ustack_t *
future_signal(future_t *future, const void *data, int size) {
  ustack_t *waiters = NULL;
  future_lock(future);
  assert(!_is_substate(future, _TRIGGERED));
  _set_value(future, data, size);
  waiters = _get_queue(future);
  _unlock(future, NULL, _TRIGGERED);
  return waiters;
}
