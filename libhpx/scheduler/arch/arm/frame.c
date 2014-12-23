#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libhpx/debug.h>
#include "../../thread.h"

static void HPX_CONSTRUCTOR _init_thread(void) {
  //thread_set_stack_size(0);
}


/// A structure describing the initial frame on a stack.
///
/// This must match the transfer.S asm file usage.
///
/// This should be managed in an asm-specific manner.
typedef struct {
  void          *r0;                            // passes initial parcel
#ifdef __VFP_FP__
  void *vfp_alignment;
  void       *fpscr;
  void *vfpregs[16];
#endif
  void     *regs[8];
  thread_entry_t lr;                            // 1 return address
} HPX_PACKED _frame_t;

static _frame_t *_get_top_frame(ustack_t *thread, size_t size) {
  int offset = size - sizeof(_frame_t);
  return (_frame_t*)((char*)thread + offset);
}

void thread_init(ustack_t *thread, hpx_parcel_t *parcel, thread_entry_t f,
                 size_t size) {
  // set up the initial stack frame
  _frame_t *frame = _get_top_frame(thread, size);
  frame->r0      = parcel;
  frame->lr      = (thread_entry_t)f;

  // set the stack stuff
  thread->sp            = frame;
  thread->next          = NULL;
  thread->parcel        = parcel;
  thread->tls_id        = -1;
  thread->affinity      = -1;
}
