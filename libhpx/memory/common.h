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


void *common_chunk_alloc(void *addr, size_t size, size_t align, bool *zero,
                         unsigned arena, void *xport, mmap_t mmap,
                         memory_register_t pin)
  HPX_INTERNAL;

bool common_chunk_dalloc(void *chunk, size_t size, unsigned arena, void *xport,
                         munmap_t munmap, memory_release_t unpin)
  HPX_INTERNAL;

typedef int (*mallctl_t)(const char *, void *, size_t *, void *, size_t);

void common_join(void *space, const void *class, unsigned *primordial_arena,
                 mallctl_t mallctl, void *alloc, void *dalloc)
  HPX_INTERNAL;

#endif
