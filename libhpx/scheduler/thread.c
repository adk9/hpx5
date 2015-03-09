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

/// User level stacks.
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>

#include <hpx/builtins.h>
#include <valgrind/valgrind.h>
#include <libhpx/debug.h>
#include <libhpx/instrumentation.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include "thread.h"

static int _buffer_size = 0;
static int _thread_size = 0;

void thread_set_stack_size(int stack_bytes) {
  assert(stack_bytes);
  int pages = ceil_div_32(stack_bytes, HPX_PAGE_SIZE);
  _thread_size = pages * HPX_PAGE_SIZE;
  _buffer_size = _thread_size;
#ifdef ENABLE_DEBUG
  assert(here && here->config);
  if (here->config->dbg_mprotectstacks)
    _buffer_size = _buffer_size + 2 * HPX_PAGE_SIZE;
#endif
}


#ifdef ENABLE_DEBUG
/// Update the protections on the first and last page in the stack.
///
/// @param         base The base address.
/// @param         prot The new permissions.
static void _mprot(void *base, int prot) {
  char *p1 = base;
  char *p2 = p1 + _thread_size + HPX_PAGE_SIZE;
  int e1 = mprotect(p1, HPX_PAGE_SIZE, prot);
  int e2 = mprotect(p2, HPX_PAGE_SIZE, prot);

  if (e1 || e2) {
    dbg_error("Mprotect error: %d (EACCES %d, EINVAL %d, ENOMEM %d)\n", errno,
              EACCES, EINVAL, ENOMEM);
  }
}
#endif

/// Protect the stack so that stack over/underflow will result in a segfault.
///
/// This returns the correct address for use in the stack structure. When we're
/// protecting the stack we want a 1-page offset here.
static ustack_t *_protect(void *base) {
  ustack_t *stack = base;
#ifdef ENABLE_DEBUG
  assert(here && here->config);
  if (!here->config->dbg_mprotectstacks) {
    return stack;
  }
  _mprot(base, PROT_NONE);
  stack = (ustack_t*)((char*)base + HPX_PAGE_SIZE);
#endif
  return stack;
}


/// Unprotect the stack so that the pages can be reused.
///
/// This returns the base address of the original allocation so that it can be
/// freed by the caller.
static void *_unprotect(ustack_t *stack) {
  void *base = stack;
#ifdef ENABLE_DEBUG
  assert(here && here->config);
  if (!here->config->dbg_mprotectstacks) {
    return base;
  }
  base = (char*)stack - HPX_PAGE_SIZE;
  _mprot(base, PROT_READ | PROT_WRITE);
#endif
  return base;
}

/// Register a stack with valgrind, so that it doesn't incorrectly complain
/// about stack accesses.
static int _register(ustack_t *thread) {
  void *begin = &thread->stack[0];
  void *end = &thread->stack[_thread_size - sizeof(*thread)];
  return VALGRIND_STACK_REGISTER(begin, end);
}

static void _deregister(ustack_t *thread) {
  VALGRIND_STACK_DEREGISTER(thread->stack_id);
}

static size_t _alignment(void) {
  dbg_assert(here && here->config);
  DEBUG_IF(here->config->dbg_mprotectstacks) {
    return HPX_PAGE_SIZE;
  }

#if defined(__ARMEL__)
  return 8;
#else
  return 16;
#endif
}

ustack_t *thread_new(hpx_parcel_t *parcel, thread_entry_t f) {
  void *base = registered_memalign(_alignment(), _buffer_size);
  dbg_assert(base);
  assert((uintptr_t)base % _alignment() == 0);

  ustack_t *thread = _protect(base);
  thread->stack_id = _register(thread);
  thread_init(thread, parcel, f, _thread_size);
  return thread;
}

void thread_delete(ustack_t *thread) {
  _deregister(thread);
  void *base = _unprotect(thread);
  registered_free(base);
}
