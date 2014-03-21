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

#include "hpx/hpx.h"

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
/// Sets the size of a stack.
///
/// All of the stacks in the system need to have the same size.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void thread_set_stack_size(int stack_bytes);

/// ----------------------------------------------------------------------------
/// Initializes a thread.
///
/// The thread can be transferred to using thread_transfer() in order to start
/// execution. The @p entry function will be run as a result of the initial
/// transfer.
///
/// @param thread - The thread to initialize.
/// @param parcel - The parcel that is generating this thread.
/// @returns      - @p thread, for convenience
/// ----------------------------------------------------------------------------
HPX_INTERNAL thread_t *thread_init(thread_t *thread, hpx_parcel_t *parcel)
  HPX_NON_NULL(1, 2);


/// ----------------------------------------------------------------------------
/// Allocates and initializes a new thread.
///
/// This allocates and initializes a new user-level stack.
///
/// @param parcel - The parcel that is generating this thread.
/// @returns      - NULL if there is an error, or a pointer to the new stack
///                 structure.
/// ----------------------------------------------------------------------------
HPX_INTERNAL thread_t *thread_new(hpx_parcel_t *parcel)
  HPX_NON_NULL(1) HPX_MALLOC;


/// ----------------------------------------------------------------------------
/// Deletes the thread.
///
/// @param thread - The thread pointer.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void thread_delete(thread_t *stack) HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Get the thread for a particular stack address.
/// ----------------------------------------------------------------------------
HPX_INTERNAL thread_t *thread_from_sp(void *sp);


/// ----------------------------------------------------------------------------
/// Get the current thread.
/// ----------------------------------------------------------------------------
HPX_INTERNAL thread_t *thread_current(void);


/// ----------------------------------------------------------------------------
/// Get the current parcel.
/// ----------------------------------------------------------------------------
HPX_INTERNAL hpx_parcel_t *thread_current_parcel(void)
  HPX_RETURNS_NON_NULL;


/// ----------------------------------------------------------------------------
/// Exit the current user-level thread, possibly with a return value.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void thread_exit(int status, const void *value, size_t size)
  HPX_NORETURN;


typedef int (*thread_transfer_cont_t)(void *, void *);

/// ----------------------------------------------------------------------------
/// The actual routine to transfer between thread.
///
/// This pushes the callee-saves state on the current stack, and swaps the stack
/// pointer to @p t->sp. It calls the provided continuation with the old stack
/// pointer, passing through the provided environment, which might be null.
///
/// The continuation's return value is also returned by transfer.
///
/// @param   t - the thread to transfer to (really void **sp)
/// @param env - the environment for the continuation
/// @param   c - a continuation function to handle the old stack pointer
/// ----------------------------------------------------------------------------
HPX_INTERNAL int thread_transfer(thread_t *t, void *env, thread_transfer_cont_t c)
  HPX_NON_NULL(1, 3);


#endif  // LIBHPX_THREAD_H
