// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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

/// @file thread.h
/// @brief Defines the lightweight thread stack structure and interface for user
///        level threads.
#include "hpx/hpx.h"

/// Forward declarations
/// @{
struct lco_class;
/// @}

typedef struct ustack {
  void             *sp;                         // checkpointed stack pointer
  hpx_parcel_t *parcel;                         // the progenitor parcel
  struct ustack  *next;                         // freelists and condition vars
  int        lco_depth;                         // how many lco locks we hold
  int           tls_id;
  int         stack_id;
  short       affinity;                         // set by user
  char           stack[];
} ustack_t;

/// This is the type of an HPX thread entry function.
typedef void (*thread_entry_t)(hpx_parcel_t *);

/// Sets the size of a stack.
///
/// All of the stacks in the system need to have the same size.
void thread_set_stack_size(int stack_bytes)
  HPX_INTERNAL;

/// Initializes a thread.
///
/// The thread can be transferred to using thread_transfer() in order to start
/// execution. The @p entry function will be run as a result of the initial
/// transfer.
///
/// @param       thread The thread to initialize.
/// @param       parcel The parcel that is generating this thread.
/// @param            f The entry function for the thread.
void thread_init(ustack_t *stack, hpx_parcel_t *parcel, thread_entry_t f,
                 size_t size) HPX_INTERNAL HPX_NON_NULL(1, 2);

/// Allocates and initializes a new thread.
///
/// This allocates and initializes a new user-level thread. User-level threads
/// are allocated in the global address space.
///
/// @param       parcel The parcel that is generating this thread.
/// @param            f The entry function for the thread.
///
/// @returns A new thread that can be transferred to.
ustack_t *thread_new(hpx_parcel_t *parcel, thread_entry_t f)
  HPX_INTERNAL HPX_NON_NULL(1) HPX_MALLOC;

/// Deletes the thread.
///
/// @param thread - The thread pointer.
void thread_delete(ustack_t *stack)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Exit the current user-level thread, possibly with a return value.
void thread_exit(int status, const void *value, size_t size)
  HPX_INTERNAL HPX_NORETURN;

/// The transfer continuation function type.
typedef int (*thread_transfer_cont_t)(hpx_parcel_t *p, void *sp, void *env);

/// The actual routine to transfer between thread.
///
/// This pushes the callee-saves state on the current stack, and swaps the stack
/// pointer to @p t->sp. It calls the provided continuation with the old stack
/// pointer, passing through the provided environment, which might be null.
///
/// The continuation's return value is also returned by transfer.
///
/// @param          p The parcel to transfer to.
/// @param       cont A continuation function to handle the old stack pointer.
/// @param        env The environment for the continuation.
///
/// @returns HPX_SUCCESS or an error code
int thread_transfer(hpx_parcel_t *p, thread_transfer_cont_t cont, void *env)
  HPX_INTERNAL HPX_NON_NULL(1, 2);

/// This transfer wrapper is used for logging, debugging, and instrumentation.
///
/// Internally, it will perform it's pre-transfer operations, call
/// thread_transfer(), and then perform post-transfer operations on the return.
int debug_transfer(hpx_parcel_t *p, thread_transfer_cont_t cont, void *env)
  HPX_INTERNAL HPX_NON_NULL(1, 2);

#if defined(ENABLE_INSTRUMENTATION) || \
  defined(ENABLE_DEBUG) ||             \
  defined(ENABLE_LOGGING)
# define thread_transfer debug_transfer
#else
# define thread_transfer thread_transfer
#endif

#endif  // LIBHPX_THREAD_H
