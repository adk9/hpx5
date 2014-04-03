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

#include <contrib/uthash/src/utlist.h>

#include "libhpx/action.h"
#include "libhpx/builtins.h"
#include "libhpx/debug.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "asm.h"
#include "thread.h"


#define _PAGE_SIZE 4096
#define _DEFAULT_PAGES 4


static int _thread_size = 0;
static int _thread_alignment = 0;
static int _stack_size = 0;
static uint32_t _mxcsr = 0;
static uint16_t _fpucw = 0;


static void HPX_CONSTRUCTOR _init_thread(void) {
  get_mxcsr(&_mxcsr);
  get_fpucw(&_fpucw);
  thread_set_stack_size(0);
}

/// ----------------------------------------------------------------------------
/// This is the type of an HPX thread entry function.
/// ----------------------------------------------------------------------------
typedef void (*thread_entry_t)(hpx_parcel_t *) HPX_NORETURN;


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


static _frame_t *_get_top_frame(thread_t *thread) {
  return (_frame_t*)&thread->stack[_stack_size - sizeof(_frame_t)];
}


static void HPX_NORETURN _thread_enter(hpx_parcel_t *parcel) {
  hpx_action_t action = parcel->action;
  hpx_action_handler_t handler = action_lookup(action);
  int status = handler(&parcel->data);
  switch (status) {
   default:
    dbg_error("action produced unhandled error, %i\n", (int)status);
    hpx_shutdown(status);
   case HPX_ERROR:
    dbg_error("action produced error\n");
    hpx_shutdown(status);
   case HPX_RESEND:
   case HPX_SUCCESS:
    hpx_thread_continue(0, NULL);
  }
  unreachable();
}


void
thread_set_stack_size(int stack_bytes) {
  if (!stack_bytes) {
    thread_set_stack_size(_PAGE_SIZE * _DEFAULT_PAGES);
    return;
  }

  // don't care about performance
  int pages = stack_bytes / _PAGE_SIZE;
  pages += (stack_bytes % _PAGE_SIZE) ? 1 : 0;
  _thread_size = _PAGE_SIZE * pages;
  _thread_alignment = 1 << ctzl(_thread_size);
  if (_thread_alignment < _thread_size)
    _thread_alignment <<= 1;
  _stack_size = _thread_size - sizeof(thread_t);
}


thread_t *
thread_init(thread_t *thread, hpx_parcel_t *parcel) {
  // set up the initial stack frame
  _frame_t *frame = _get_top_frame(thread);
  frame->mxcsr   = _mxcsr;
  frame->fpucw   = _fpucw;
  frame->rdi     = parcel;
  frame->rbp     = &frame->rip;
  frame->rip     = _thread_enter;

  // set up the thread information
  thread->sp     = frame;
  thread->parcel = parcel;
  thread->next   = NULL;

  // set up the parcel information
  // parcel->thread = thread;
  return thread;
}


thread_t *thread_new(hpx_parcel_t *parcel) {
  // try to get a freelisted thread, or allocate a new, properly-aligned one
  thread_t *t = malloc(_thread_size);
  // if (posix_memalign((void**)&t, _thread_alignment, _thread_size))
  assert(t);
  return thread_init(t, parcel);
}


void thread_delete(thread_t *thread) {
  free(thread);
}


void hpx_thread_continue(size_t size, const void *value) {
  // if there's a continuation future, then we set it, which could spawn a
  // message if the future isn't local
  hpx_parcel_t *parcel = scheduler_current_parcel();
  hpx_addr_t cont = parcel->cont;
  if (!hpx_addr_eq(cont, HPX_NULL))
    hpx_lco_set(cont, value, size, HPX_NULL);   // async

  // exit terminates this thread
  scheduler_exit(parcel);
}


void hpx_thread_exit(int status) {
  assert(status == HPX_SUCCESS);

  // if there's a continuation future, then we set it, which could spawn a
  // message if the future isn't local
  hpx_parcel_t *parcel = scheduler_current_parcel();
  hpx_addr_t cont = parcel->cont;
  if (!hpx_addr_eq(cont, HPX_NULL))
    hpx_lco_set(cont, NULL, 0, HPX_NULL);   // async

  // exit terminates this thread
  scheduler_exit(parcel);
}
