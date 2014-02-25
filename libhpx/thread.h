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
#ifndef LIBHPX_THREAD_H
#define LIBHPX_THREAD_H

/// ----------------------------------------------------------------------------
/// @file stack.h
/// @brief Defines the stack structure and interface for user level threads.
/// ----------------------------------------------------------------------------

#include "hpx.h"

/// ----------------------------------------------------------------------------
/// Initializes the stack subsystem.
/// ----------------------------------------------------------------------------
HPX_INTERNAL int thread_init_module(int stack_size);
HPX_INTERNAL void thread_fini_module(void);

/// ----------------------------------------------------------------------------
/// A user level thread.
///
/// @field     sp - the checkpointed stack pointer for thread_transfer()
/// @field parcel - the parcel associated with the thread
/// @field   next - a list pointer
/// @field  stack - the actual stack bytes (grows down)
/// ----------------------------------------------------------------------------
typedef struct thread thread_t;
struct thread {
  void *sp;
  hpx_parcel_t *parcel;
  thread_t *next;
  char stack[];
};

/// ----------------------------------------------------------------------------
/// The type of the stack entry function.
///
/// Each thread is initialized with an initial entry function in
/// thread_new(). This is the type of that function. The first thread_transfer()
/// to the stack will look as if it is a function call to the entry.
/// ----------------------------------------------------------------------------
typedef void (*thread_entry_t)(hpx_parcel_t *) HPX_NORETURN;

/// ----------------------------------------------------------------------------
/// Initializes a thread.
///
/// The thread can be transferred to using thread_transfer() in order to start
/// execution. The @p entry function will be run as a result of the initial
/// transfer.
///
/// @param thread - The thread to initialize.
/// @param  entry - The function that will run when this thread is first
///                 thread_transfer()red to.
/// @param parcel - The parcel that is generating this thread.
/// @returns      - @p thread, for convenience
/// ----------------------------------------------------------------------------
HPX_INTERNAL thread_t *thread_init(thread_t *thread, thread_entry_t entry,
                                   hpx_parcel_t *parcel)
  HPX_NON_NULL(1, 2, 3);

/// ----------------------------------------------------------------------------
/// Allocates and initializes a new thread.
///
/// This allocates and initializes a new user-level stack.
///
/// @param  entry - The function that will run when this stack is first
///                 thread_transfer()red to.
/// @param parcel - The parcel that is generating this thread.
/// @returns      - NULL if there is an error, or a pointer to the new stack
///                 structure.
/// ----------------------------------------------------------------------------
HPX_INTERNAL thread_t *thread_new(thread_entry_t entry, hpx_parcel_t *parcel)
  HPX_NON_NULL(1, 2) HPX_MALLOC;

/// ----------------------------------------------------------------------------
/// Deletes the thread.
///
/// @param thread - The thread pointer.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void thread_delete(thread_t *stack) HPX_NON_NULL(1);

/// ----------------------------------------------------------------------------
/// Get the current thread.
/// ----------------------------------------------------------------------------
HPX_INTERNAL thread_t *thread_current(void);

/// ----------------------------------------------------------------------------
/// The actual routine to transfer between thread.
///
/// This pushes the callee-saves state on the current stack, and swaps the stack
/// pointer to @p sp. It calls the provided continuation with the old stack
/// pointer, passing through the provided environment, which might be null.
///
/// The continuation's return value is also returned by transfer.
///
/// @param  sp - the stack pointer to transfer to
/// @param env - the environment for the continuation
/// @param   c - a continuation function to handle the old stack pointer
/// ----------------------------------------------------------------------------
HPX_INTERNAL int thread_transfer(void *sp, void *env, int (*c)(void *sp, void*))
  HPX_NON_NULL(1, 3);

/// ----------------------------------------------------------------------------
/// These two routines checkpoint the stack pointer in the current (at the time
/// of the thread_transfer call entry) thread, and then push the current thread
/// onto the passed stack, either using a locked push, or not.
///
/// The locked push assumes that the @p stack is using least-significant bit
/// locking, and that it might also be using the other two least-significant
/// bits to store state. It will preserve this data during the push, with the
/// caveat that it will unset the least significant bit to unlock the stack.
///
/// @param    sp - the stack to push the current thread on (thread_t **)
/// @param stack - an (optionally) locked stack to push the current thread onto
/// ----------------------------------------------------------------------------
/// @{
HPX_INTERNAL int thread_transfer_push(void *sp, void *stack);
HPX_INTERNAL int thread_transfer_push_unlock(void *sp, void *stack);
/// @}

#endif  // LIBHPX_THREAD_H
