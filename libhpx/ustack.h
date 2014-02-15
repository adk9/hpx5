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
#ifndef LIBHPX_USTACK_H
#define LIBHPX_USTACK_H

/// ----------------------------------------------------------------------------
/// @file stack.h
/// @brief Defines the stack structure and interface for user level threads.
/// ----------------------------------------------------------------------------

#include "hpx.h"

/// ----------------------------------------------------------------------------
/// Initializes the stack subsystem.
/// ----------------------------------------------------------------------------
HPX_INTERNAL int ustack_init(void);
HPX_INTERNAL int ustack_init_thread(void);

/// ----------------------------------------------------------------------------
/// A user level stack.
///
/// @field     sp - the checkpointed stack pointer for ustack_transfer()
/// @field parcel - the parcel associated with the stack
/// @field  stack - the actual stack bytes (grows down)
/// ----------------------------------------------------------------------------
typedef struct ustack ustack_t;
struct ustack {
  void *sp;
  hpx_parcel_t *parcel;
  ustack_t *next;
  char stack[];
};

/// ----------------------------------------------------------------------------
/// The type of the stack entry function.
///
/// Each stack is initialized with an initial entry function in
/// ustack_new(). This is the type of that function. The first ustack_transfer()
/// to the stack will look as if it is a function call to the entry.
/// ----------------------------------------------------------------------------
typedef void (*ustack_entry_t)(hpx_parcel_t *) HPX_NORETURN;

/// ----------------------------------------------------------------------------
/// Allocates and initializes a new stack.
///
/// This allocates and initializes a new user-level stack. The stack can be
/// transferred to using ustack_transfer() in order to start execution. The
/// @p entry function will be run as a result of the initial transfer.
///
/// @param  entry - The function that will run when this stack is first
///                 ustack_transfer()red to.
/// @param parcel - The parcel that is generating this thread.
/// @returns      - NULL if there is an error, or a pointer to the new stack
///                 structure.
/// ----------------------------------------------------------------------------
HPX_INTERNAL ustack_t *ustack_new(ustack_entry_t entry, hpx_parcel_t *parcel)
  HPX_NON_NULL(1) HPX_MALLOC;

/// ----------------------------------------------------------------------------
/// Deletes the stack.
///
/// @param stack - The stack pointer.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void ustack_delete(ustack_t *stack) HPX_NON_NULL(1);

/// ----------------------------------------------------------------------------
/// Get the current stack.
/// ----------------------------------------------------------------------------
HPX_INTERNAL ustack_t *ustack_current(void);

/// ----------------------------------------------------------------------------
/// The actual routine to transfer between stacks.
///
/// This pushes the callee-saves state on the current state, swaps the stack
/// pointer to @p sp, calls @p c(@p sp) after the swap, and then pops the
/// callee-saves state from the new stack.
///
/// The continuation's return value is also returned by transfer.
///
/// @param sp - the stack pointer to transfer to
/// @param  c - a continuation function to handle the old stack pointer
/// ----------------------------------------------------------------------------
HPX_INTERNAL int ustack_transfer(void *sp, int (*c)(void*)) HPX_NON_NULL(1, 2);

/// ----------------------------------------------------------------------------
/// These three routines serve as default possible transfer continuations.
/// ----------------------------------------------------------------------------
/// @{
HPX_INTERNAL int ustack_transfer_delete(void *sp);
HPX_INTERNAL int ustack_transfer_checkpoint(void *sp);
/// @}

#endif  // LIBHPX_USTACK_H
