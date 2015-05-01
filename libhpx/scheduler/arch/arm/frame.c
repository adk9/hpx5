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
#include "asm.h"

static void HPX_CONSTRUCTOR _init_thread(void) {
  //thread_set_stack_size(0);
}


/// A structure describing the initial frame on a stack.
///
/// This must match the transfer.S asm file usage.
///
/// This should be managed in an asm-specific manner.
typedef struct {
  void     *alignment; // keep b-byte aligned stack
#ifdef __VFP_FP__
  void *vfp_alignment;
  void         *fpscr;
  void   *vfpregs[16];
#endif
  void       *regs[6]; // r6-r11
  void            *r5; // we use this to hold the parcel that is passed to f()
  thread_entry_t   r4; // used to hold f(), called by align_stack_trampoline
  void    (*lr)(void); // return address - set to align_stack_trampoline
} HPX_PACKED _frame_t;

static _frame_t *_get_top_frame(ustack_t *thread, size_t size) {
  int offset = size - sizeof(_frame_t);
  return (_frame_t*)((char*)thread + offset);
}

void thread_init(ustack_t *thread, hpx_parcel_t *parcel, thread_entry_t f,
                 size_t size) {
  // set up the initial stack frame
  _frame_t *frame = _get_top_frame(thread, size);
  assert((uintptr_t)frame % 8 == 0);
  frame->r4      = (thread_entry_t)f;
  frame->r5      = parcel;
  frame->lr      = align_stack_trampoline;
  frame->regs[1] = parcel;

  // set the stack stuff
  thread->sp        = frame;
  thread->next      = NULL;
  thread->parcel    = parcel;
  thread->lco_depth = 0;
  thread->tls_id    = -1;
  thread->affinity  = -1;
}
