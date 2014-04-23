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
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <contrib/uthash/src/utlist.h>

#include "hpx/builtins.h"
#include "libhpx/debug.h"
#include "libhpx/scheduler.h"
#include "asm.h"
#include "thread.h"


#define _DEFAULT_PAGES 4


static int _thread_size = 0;
static uint32_t  _mxcsr = 0;
static uint16_t  _fpucw = 0;


static void HPX_CONSTRUCTOR _init_thread(void) {
  get_mxcsr(&_mxcsr);
  get_fpucw(&_fpucw);
  thread_set_stack_size(0);
}


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


static _frame_t *_get_top_frame(ustack_t *stack) {
  return (_frame_t*)((char*)stack + _thread_size - sizeof(_frame_t));
}


void
thread_set_stack_size(int stack_bytes) {
  if (!stack_bytes) {
    thread_set_stack_size(HPX_PAGE_SIZE * _DEFAULT_PAGES);
    return;
  }

  // don't care about performance
  int pages = stack_bytes / HPX_PAGE_SIZE;
  pages += (stack_bytes % HPX_PAGE_SIZE) ? 1 : 0;
  _thread_size = HPX_PAGE_SIZE * pages;
}


void thread_init(ustack_t *stack, hpx_parcel_t *parcel, thread_entry_t f) {
  // set up the initial stack frame
  _frame_t *frame = _get_top_frame(stack);
  frame->mxcsr   = _mxcsr;
  frame->fpucw   = _fpucw;
  frame->rdi     = parcel;
  frame->rbp     = &frame->rip;
  frame->rip     = f;

  // set the stack stuff
  stack->sp      = frame;
  stack->tls_id  = -1;
}

ustack_t *thread_new(hpx_parcel_t *parcel, thread_entry_t f) {
  ustack_t *stack = valloc(_thread_size);
  assert(stack);
  thread_init(stack, parcel, f);
  return stack;
}

void thread_delete(ustack_t *stack) {
  free(stack);
}

#if 0
#include "libsync/sync.h"

static uint64_t _stacks = 0;

ustack_t *thread_new(hpx_parcel_t *parcel, thread_entry_t f) {
  sync_fadd(&_stacks, 1, SYNC_ACQ_REL);
  // Allocate a page-aligned thread structure, along with a guard page to detect
  // stack overflow.
  char *m = valloc(HPX_PAGE_SIZE + _thread_size);
  assert(m);

  // set up the guard page at the top of the thread structure
  int e = mprotect((void*)m, HPX_PAGE_SIZE, PROT_NONE);
  if (e) {
    uint64_t stacks;
    sync_load(stacks, &_stacks, SYNC_ACQUIRE);
    dbg_error("failed to mark a guard page for the thread, %lu.\n", stacks);
    hpx_abort();
  }

  ustack_t *stack = (ustack_t*)(m + HPX_PAGE_SIZE);
  thread_init(stack, parcel, f);
  return stack;
}

void thread_delete(ustack_t *stack) {
  char *block = (char*)stack - HPX_PAGE_SIZE;
  int e = mprotect(block, HPX_PAGE_SIZE, PROT_READ | PROT_WRITE);
  if (e) {
    dbg_error("failed to unprotect a guard page.\n");
    // don't abort
  }
  free(block);
}

#endif
