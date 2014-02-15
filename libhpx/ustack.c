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
#include "ustack.h"
#include "parcel.h"
#include "asm.h"

/// ----------------------------------------------------------------------------
/// Stack sizes.
///
/// @todo These need to be configurable at runtime.
/// ----------------------------------------------------------------------------
#define LIBHPX_USTACK_PAGE_SIZE 4096
#define LIBHPX_USTACK_PAGES 4
#define LIBHPX_USTACK_SIZE (LIBHPX_USTACK_PAGE_SIZE * LIBHPX_USTACK_PAGES)

/// ----------------------------------------------------------------------------
/// A structure describing the initial frame on a stack.
///
/// This must match the ustack_transfer.S asm file usage.
///
/// This should be managed in an asm-specific manner, but we are just worried
/// about x86-64 at the moment.
/// ----------------------------------------------------------------------------
#ifndef __x86_64__
#error No stack frame for your architecture
#else
typedef struct {
  uint32_t     mxcsr;
  uint16_t     fpucw;
  uint16_t   padding;                           // has to match transfer.S
  void          *rdi;                           // passes initial parcel
  void          *r15;
  void          *r14;
  void          *r13;
  void          *r12;
  void          *rbx;
  void          *rbp;
  ustack_entry_t rip;
  void    *alignment[1];
} HPX_PACKED _frame_t;
#endif

/// ----------------------------------------------------------------------------
/// Captured for new stack allocation in ustack_init_thread().
/// ----------------------------------------------------------------------------
/// @{
static __thread uint32_t _mxcsr = 0;
static __thread uint16_t _fpucw = 0;
/// @}

/// ----------------------------------------------------------------------------
/// Freelist for thread-local stacks.
/// ----------------------------------------------------------------------------
static __thread ustack_t *_stacks = NULL;

static _frame_t *_get_top_frame(ustack_t *stack) {
  assert(sizeof(_frame_t) == 80);
  return (_frame_t*)&stack->stack[LIBHPX_USTACK_SIZE - sizeof(ustack_t) -
                                  sizeof(_frame_t)];
}

static ustack_t *_get_from_sp(void *sp) {
  const uintptr_t MASK = ~(uintptr_t)0 << __builtin_ctzl(LIBHPX_USTACK_SIZE);
  uintptr_t addr = (uintptr_t)sp;
  addr &= MASK;
  ustack_t *stack = (ustack_t*)addr;
  return stack;
}

int
ustack_init(void) {
  return HPX_SUCCESS;
}

int
ustack_init_thread(void) {
  get_mxcsr(&_mxcsr);
  get_fpucw(&_fpucw);
  return HPX_SUCCESS;
}

ustack_t *
ustack_new(ustack_entry_t entry, hpx_parcel_t *parcel) {
  // try to get a freelisted stack, or allocate a new, properly-aligned one
  ustack_t *stack = _stacks;
  if (stack) {
    _stacks = _stacks->next;
  }
  else {
    if (posix_memalign((void**)&stack, LIBHPX_USTACK_SIZE, LIBHPX_USTACK_SIZE))
      assert(false);
  }

  // set up the initial stack frame
  _frame_t *frame = _get_top_frame(stack);
  frame->mxcsr  = _mxcsr;
  frame->fpucw  = _fpucw;
  frame->rdi    = parcel;
  frame->rbp    = &frame->rip;
  frame->rip    = entry;

  // set up the stack information
  stack->sp     = frame;
  stack->parcel = parcel;
  stack->next   = NULL;

  // set up the parcel information
  parcel->stack = stack;

  return stack;
}

ustack_t *
ustack_current(void) {
  return _get_from_sp(get_sp());
}

void
ustack_delete(ustack_t *stack) {
  stack->next = _stacks;
  _stacks = stack;
}

int
ustack_transfer_delete(void *sp) {
  ustack_t *stack = _get_from_sp(sp);
  ustack_delete(stack);
  return HPX_SUCCESS;
}

int
ustack_transfer_checkpoint(void *sp) {
  ustack_t *stack = _get_from_sp(sp);
  stack->sp = sp;
  return HPX_SUCCESS;
}
