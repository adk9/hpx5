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
#ifndef LIBHPX_MEMORY_COMMON_H
#define LIBHPX_MEMORY_COMMON_H

#include <hpx/attributes.h>
#include <libhpx/memory.h>

/// This file declares a common interface for jemalloc address spaces.
typedef struct {
  address_space_t vtable;
  void *xport;
  memory_register_t pin;
  memory_release_t unpin;
  void *mmap_obj;
  system_mmap_t mmap;
  system_munmap_t munmap;
  int (*mallctl)(const char *, void *, size_t *, void *, size_t);
} common_allocator_t;

void *common_chunk_alloc(void *common, void *addr, size_t size, size_t align,
                         bool *zero, unsigned arena)
  HPX_INTERNAL;

bool common_chunk_dalloc(void *common, void *chunk, size_t size, unsigned arena)
  HPX_INTERNAL;

void common_join(void *common, unsigned *arena, void *alloc, void *dalloc)
  HPX_INTERNAL;

void common_leave(void *common)
  HPX_INTERNAL;

void common_delete(void *common)
  HPX_INTERNAL;

#endif
