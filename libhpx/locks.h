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
#ifndef LIBHPX_LOCKS_H
#define LIBHPX_LOCKS_H

/// ----------------------------------------------------------------------------
/// @file locks.h
///
/// This file contains a collection of locks and synchronization that we use in
/// libhpx. These locks are more specific than the functionality provided in
/// sync/locks.h, in particular, they are scheduler aware---an important
/// consideration in our lightweight-threading model.
///
/// ----------------------------------------------------------------------------
#include <stdbool.h>
#include "attributes.h"

/// ----------------------------------------------------------------------------
///
/// ----------------------------------------------------------------------------
HPX_INTERNAL void packed_ptr_lock(void **ptr);
HPX_INTERNAL void packed_ptr_unlock(void **ptr);
HPX_INTERNAL void packed_ptr_set(void **ptr, uintptr_t state);
HPX_INTERNAL bool packed_ptr_is_set(const void *ptr, uintptr_t state);

HPX_INTERNAL void lockable_packed_stack_push_and_unlock(void **stack, void *element, void **next);
HPX_INTERNAL void *lockable_packed_stack_pop_all_and_unlock(void **stack);

#define LOCKABLE_PACKED_STACK_PUSH_AND_UNLOCK(stack, element)           \
  lockable_packed_stack_push_and_unlock((void**)(stack), (element), (void**)&(element)->next)

#define LOCKABLE_PACKED_STACK_POP_ALL_AND_UNLOCK(stack)                 \
  (__typeof__(*(stack))) lockable_packed_stack_pop_all_and_unlock((void**)stack)

/// @}

#endif // LIBHPX_LOCKS_H
