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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libhpx/debug.h>
#include "../../thread.h"
#include "../common/asm.h"

/// A structure describing the initial frame on a stack.
///
/// This must match the transfer.S asm file usage.
///
/// This should be managed in an asm-specific manner.
typedef struct {
#ifdef __VFP_FP__
  void  *vfp_alignment;
  void          *fpscr;
  void     *vfpregs[8];
#endif
  thread_entry_t    x19; // used to hold f(), called by align_stack_trampoline
  void             *x20; // we use this to hold the parcel that is passed to f()
  void         *regs[8]; // x21-x28
  void             *x29; // The frame pointer
  void     (*x30)(void); // return address - set to align_stack_trampoline
#ifdef ENABLE_DEBUG
  void         *top_x19;
  void (*top_x30)(void);
#endif
} HPX_PACKED _frame_t;

static _frame_t *_get_top_frame(ustack_t *thread, size_t size) {
  int offset = size - sizeof(_frame_t);
  return (_frame_t*)((char*)thread + offset);
}

void thread_init(ustack_t *thread, hpx_parcel_t *parcel, thread_entry_t f,
                 size_t size) {
  // set up the initial stack frame
  _frame_t *frame = _get_top_frame(thread, size);
  assert((uintptr_t)frame % 16 == 0);
  frame->x19      = (thread_entry_t)f; // register must be the same as the one
                                      // in align_stack_trampoline
  frame->x20      = parcel;
  frame->x30      = align_stack_trampoline;

#ifdef ENABLE_DEBUG
  frame->top_x19 = NULL;
  frame->top_x30 = NULL;
#endif

  // set the stack stuff
  thread->sp        = frame;
  thread->next      = NULL;
  thread->parcel    = parcel;
  thread->lco_depth = 0;
  thread->tls_id    = -1;
  thread->size      = size;
  thread->affinity  = -1;
}
