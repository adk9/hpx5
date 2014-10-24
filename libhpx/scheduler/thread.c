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

#include "thread.h"

#define _DEFAULT_PAGES 4

static int _thread_size = 0;

void thread_set_stack_size(int stack_bytes) {
  if (!stack_bytes) {
    thread_set_stack_size(HPX_PAGE_SIZE * _DEFAULT_PAGES);
    return;
  }

  // don't care about performance
  int pages = stack_bytes / HPX_PAGE_SIZE;
  pages += (stack_bytes % HPX_PAGE_SIZE) ? 1 : 0;
  _thread_size = HPX_PAGE_SIZE * pages;
}

ustack_t *thread_new(hpx_parcel_t *parcel, thread_entry_t f) {
  ustack_t *stack = NULL;
  int e = posix_memalign((void**)&stack, 16, _thread_size);
  assert(!e);
  thread_init(stack, parcel, f, _thread_size);
  return stack;
}

void thread_delete(ustack_t *stack) {
  free(stack);
}
