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
#include <stdint.h>
#include "locks.h"
#include "scheduler.h"
#include "sync/sync.h"

#define _LOCK_MASK (0x1)
#define _UNLOCK_MASK ~_LOCK_MASK
#define _STATE_MASK (0x7)
#define _ADDR_MASK ~_STATE_MASK

static uintptr_t _get_state(void *addr) {
  return (uintptr_t)addr & _STATE_MASK;
}

static void *_get_addr(void *addr) {
  return (void*)((uintptr_t)addr & _ADDR_MASK);
}

static uintptr_t _as_unlocked(uintptr_t state) {
  return state & _UNLOCK_MASK;
}

static bool _is_locked(uintptr_t state) {
  return state & _LOCK_MASK;
}

static uintptr_t _as_locked(uintptr_t state) {
  return state | _LOCK_MASK;
}

static void *_set_state(void *addr, uintptr_t state) {
  return (void*)((uintptr_t)addr | state);
}


void
lockable_packed_stack_push_and_unlock(void **stack, void *element, void **next) {
    void *top = *stack;
    *next = _get_addr(top);
    uintptr_t state = _get_state(top);
    state = _as_unlocked(state);
    element = _set_state(element, state);
    sync_store(stack, element, SYNC_RELEASE);
}

void *
lockable_packed_stack_pop_all_and_unlock(void **stack) {
  void *top = *stack;
  uintptr_t state = _get_state(top);
  state = _as_unlocked(state);
  sync_store(stack, _set_state(NULL, state), SYNC_RELEASE);
  return _get_addr(top);
}


void
packed_ptr_lock(void **ptr) {
  while (true) {
    void *from = *ptr;
    uintptr_t state = _get_state(from);

    if (_is_locked(state)) {
      scheduler_yield();
      continue;
    }

    state = _as_locked(state);
    void *to = _get_addr(from);
    to = _set_state(to, state);

    if (!sync_cas(ptr, from, to, SYNC_ACQUIRE, SYNC_RELAXED)) {
      scheduler_yield();
      continue;
    }

    return;
  }
}

void
packed_ptr_unlock(void **ptr) {
  void *val = *ptr;
  uintptr_t state = _get_state(val);
  state = _as_unlocked(state);
  void *to = _get_addr(val);
  to = _set_state(to, state);
  sync_store(ptr, to, SYNC_RELEASE);
}

void
packed_ptr_set(void **ptr, uintptr_t state) {
  *ptr = (void*)((uintptr_t)*ptr | state);
}

bool
packed_ptr_is_set(const void *ptr, uintptr_t state) {
  return (uintptr_t)ptr & state;
}
