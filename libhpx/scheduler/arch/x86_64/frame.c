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
# include "config.h"
#endif

#include <libhpx/debug.h>
#include "../../thread.h"
#include "asm.h"


static uint32_t  _mxcsr = 0;
static uint16_t  _fpucw = 0;


HPX_CONSTRUCTOR
static void _init_thread(void) {
  get_mxcsr(&_mxcsr);
  get_fpucw(&_fpucw);
  thread_set_stack_size(0);
}


/// A structure describing the initial frame on a stack.
///
/// This must match the transfer.S asm file usage.
///
/// This should be managed in an asm-specific manner, but we are just worried
/// about x86-64 at the moment.
typedef struct {
  uint32_t     mxcsr;                           // 7
  uint16_t     fpucw;                           // 7.5
  uint16_t   padding;                           // 7.75 has to match transfer.S
  void          *r15;                           // 6
  void          *r14;                           // 5
  void          *r13;                           // 4
  hpx_parcel_t  *r12;                           // 3
  thread_entry_t rbx;                           // 2
  void          *rbp;                           // 1
  void         (*rip)(void);                    // 0
#ifdef ENABLE_DEBUG
  void     (*top_rip)(void);
  void        *align;
#endif
} HPX_PACKED _frame_t;


static _frame_t *_get_top_frame(ustack_t *stack, size_t size) {
  return (_frame_t*)((char*)stack + size - sizeof(_frame_t));
}


void thread_init(ustack_t *stack, hpx_parcel_t *parcel, thread_entry_t f,
                 size_t size) {
  // set up the initial stack frame
  _frame_t *frame = _get_top_frame(stack, size);
  frame->mxcsr   = _mxcsr;
  frame->fpucw   = _fpucw;

#ifdef ENABLE_DEBUG
  frame->r15 = NULL;
  frame->r14 = NULL;
  frame->r13 = NULL;
#endif

  frame->r12 = parcel;
  frame->rbx = f;
  frame->rbp = &frame->rip;
  frame->rip = align_stack;

#ifdef ENABLE_DEBUG
  frame->top_rip = align_stack;
  frame->align = NULL;
#endif

  // set the stack stuff
  stack->sp            = frame;
  stack->next          = NULL;
  stack->parcel        = parcel;
  stack->tls_id        = -1;
  stack->affinity      = -1;
}
