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

#include "libhpx/action.h"
#include "libhpx/builtins.h"
#include "libhpx/debug.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "asm.h"
#include "thread.h"


#define _DEFAULT_PAGES 4


static int _thread_size = 0;
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
  return (_frame_t*)&thread->stack[_thread_size - sizeof(thread_t) - sizeof(_frame_t)];
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
    thread_set_stack_size(HPX_PAGE_SIZE * _DEFAULT_PAGES);
    return;
  }

  // don't care about performance
  int pages = stack_bytes / HPX_PAGE_SIZE;
  pages += (stack_bytes % HPX_PAGE_SIZE) ? 1 : 0;
  _thread_size = HPX_PAGE_SIZE * pages;
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

#include "libsync/sync.h"

static uint64_t _stacks = 0;


thread_t *thread_new(hpx_parcel_t *parcel) {
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
  thread_t *t = (thread_t *)(m + HPX_PAGE_SIZE);
  return thread_init(t, parcel);
}

void thread_delete(thread_t *thread) {
  char *block = (char*)thread - HPX_PAGE_SIZE;
  int e = mprotect(block, HPX_PAGE_SIZE, PROT_READ | PROT_WRITE);
  if (e) {
    dbg_error("failed to unprotect a guard page.\n");
    // don't abort
  }
  free(block);
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
