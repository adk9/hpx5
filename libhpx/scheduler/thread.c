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

/// User level stacks.
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(ENABLE_DEBUG) && defined(LIBHPX_DBG_PROTECT_STACK)
#include <sys/mman.h>
#include <errno.h>
#endif

#include <hpx/builtins.h>
#include <jemalloc/jemalloc.h>
#include <valgrind/valgrind.h>
#include <libhpx/debug.h>
#include "thread.h"

#ifdef ENABLE_TAU
#define TAU_DEFAULT 1
#include <TAU.h>
#endif

#define _DEFAULT_PAGES 4

static int _thread_size = 0;

void thread_set_stack_size(int stack_bytes) {
  int pages = (stack_bytes) ? ceil_div_32(stack_bytes, HPX_PAGE_SIZE) :
              _DEFAULT_PAGES;
  _thread_size = HPX_PAGE_SIZE * pages;
}

#if defined(ENABLE_DEBUG) && defined(LIBHPX_DBG_PROTECT_STACK)
static void _prot(char *base, int prot) {
  int e = mprotect(base, HPX_PAGE_SIZE, prot);
  assert(!e);
  e = mprotect(base + _thread_size + HPX_PAGE_SIZE, HPX_PAGE_SIZE, prot);
  DEBUG_IF(e) {
    dbg_error("Mprotect error: %d (EACCES %d, EINVAL %d, ENOMEM %d)\n", errno, EACCES, EINVAL, ENOMEM);
  }
  assert(!e);
}
#endif

ustack_t *thread_new(hpx_parcel_t *parcel, thread_entry_t f) {
  ustack_t *stack = NULL;
#if defined(ENABLE_DEBUG) && defined(LIBHPX_DBG_PROTECT_STACK)
  char *base = NULL;
  int e = posix_memalign((void**)&base, HPX_PAGE_SIZE, _thread_size + 2 *
                         HPX_PAGE_SIZE);
  assert(!e);
  _prot(base, PROT_NONE);
  stack = (void*)(base + HPX_PAGE_SIZE);
#else
  int e = posix_memalign((void**)&stack, 16, _thread_size);
  assert(!e);
#endif

  assert(stack);
  stack->stack_id = VALGRIND_STACK_REGISTER(&stack->stack, (char*)stack + _thread_size);

  thread_init(stack, parcel, f, _thread_size);
  return stack;
}

void thread_delete(ustack_t *stack) {
  VALGRIND_STACK_DEREGISTER(stack->stack_id);
#if defined(ENABLE_DEBUG) && defined(LIBHPX_DBG_PROTECT_STACK)
  char *base = (char*)stack - HPX_PAGE_SIZE;
  _prot(base, PROT_READ | PROT_WRITE);
  free(base);
#else
  free(stack);
#endif  // OB
}
