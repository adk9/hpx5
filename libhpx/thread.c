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
/// User level stacks.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "thread.h"
#include "parcel.h"
#include "locks.h"
#include "asm.h"

/// ----------------------------------------------------------------------------
/// Stack sizes.
///
/// @todo These need to be configurable at runtime.
/// ----------------------------------------------------------------------------
#define LIBHPX_THREAD_PAGE_SIZE 4096
#define LIBHPX_THREAD_PAGES 4
#define LIBHPX_THREAD_SIZE (LIBHPX_THREAD_PAGE_SIZE * LIBHPX_THREAD_PAGES)

/// ----------------------------------------------------------------------------
/// A structure describing the initial frame on a stack.
///
/// This must match the transfer.S asm file usage.
///
/// This should be managed in an asm-specific manner, but we are just worried
/// about x86-64 at the moment.
/// ----------------------------------------------------------------------------
#ifndef __x86_64__
#error No stack frame for your architecture
#else
typedef struct {
  uint32_t     mxcsr;                           // 8
  uint16_t     fpucw;                           // 8.5
  uint16_t   padding;                           // 8.74 has to match transfer.S
  void          *rdi;                           // 7 passes initial parcel
  void          *r15;                           // 6
  void          *r14;                           // 5
  void          *r13;                           // 4
  void          *r12;                           // 3
  void          *rbx;                           // 2
  void          *rbp;                           // 1
  thread_entry_t rip;                           // 0
  void *alignment[1];                           // offset for stack alignment
} HPX_PACKED _frame_t;
#endif

/// ----------------------------------------------------------------------------
/// Captured for new stack allocation in thread_init_thread().
/// ----------------------------------------------------------------------------
/// @{
static __thread uint32_t _mxcsr = 0;
static __thread uint16_t _fpucw = 0;
/// @}

static _frame_t *_get_top_frame(thread_t *thread) {
  assert(sizeof(_frame_t) == 80);
  return (_frame_t*)&thread->stack[LIBHPX_THREAD_SIZE - sizeof(thread_t) -
                                   sizeof(_frame_t)];
}

static thread_t *_get_from_sp(void *sp) {
  const uintptr_t MASK = ~(uintptr_t)0 << __builtin_ctzl(LIBHPX_THREAD_SIZE);
  uintptr_t addr = (uintptr_t)sp;
  addr &= MASK;
  thread_t *thread = (thread_t*)addr;
  return thread;
}

int
thread_init_module(void) {
  return HPX_SUCCESS;
}

void
thread_fini_module(void) {
}

int
thread_init_thread(void) {
  get_mxcsr(&_mxcsr);
  get_fpucw(&_fpucw);
  return HPX_SUCCESS;
}

void
thread_fini_thread(void) {
}

thread_t *
thread_init(thread_t *thread, thread_entry_t entry, hpx_parcel_t *parcel) {
  // set up the initial stack frame
  _frame_t *frame = _get_top_frame(thread);
  frame->mxcsr   = _mxcsr;
  frame->fpucw   = _fpucw;
  frame->rdi     = parcel;
  frame->rbp     = &frame->rip;
  frame->rip     = entry;

  // set up the thread information
  thread->sp     = frame;
  thread->parcel = parcel;
  thread->next   = NULL;

  // set up the parcel information
  parcel->thread = thread;
  return thread;
}

thread_t *
thread_new(thread_entry_t entry, hpx_parcel_t *parcel) {
  // try to get a freelisted thread, or allocate a new, properly-aligned one
  thread_t *t = NULL;
  if (posix_memalign((void**)&t, LIBHPX_THREAD_SIZE, LIBHPX_THREAD_SIZE))
    assert(false);
  return thread_init(t, entry, parcel);
}

thread_t *
thread_current(void) {
  return _get_from_sp(get_sp());
}

void
thread_delete(thread_t *thread) {
  free(thread);
}

int
thread_transfer_push(void *sp, void *env) {
  thread_t **stack = env;
  thread_t *thread = _get_from_sp(sp);
  thread->sp = sp;
  thread->next = *stack;
  *stack = thread;
  return HPX_SUCCESS;
}

int
thread_transfer_push_unlock(void *sp, void *env) {
  thread_t **stack = env;
  thread_t *thread = _get_from_sp(sp);
  thread->sp = sp;
  LOCKABLE_PACKED_STACK_PUSH_AND_UNLOCK(stack, thread);
  return HPX_SUCCESS;
}
