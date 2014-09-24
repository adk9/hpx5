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
#ifndef LIBHPX_GAS_BITMAP_ALLOC_H
#define LIBHPX_GAS_BITMAP_ALLOC_H

#include <stddef.h>
#include <stdint.h>
#include <hpx/attributes.h>
#include <libsync/locks.h>

/// @file libhpx/gas/bitmap_alloc.h
/// @brief Implement a simple parallel bitmap allocator to manage chunk
///        allocation for jemalloc.
///
/// We use jemalloc directly to manage our symmetric heaps. Unlike dlmalloc,
/// which provides a "memory space" abstraction, jemalloc operates in terms of
/// large "chunks," which are typically provided through the system mmap/munmap
/// functionality.
///
/// In our case, we want jemalloc to use chunks that we provide from our
/// symmetric heap.
///
/// @note Our implementation is currently single-lock based. If this becomes a
///       synchronization bottleneck we can move to a non-blocking or more
///       fine-grained approach.
///

typedef struct {
  tatas_lock_t lock;                            // single lock for now
  uint32_t      min;
  uint32_t    nbits;
  uintptr_t  bits[];
} bitmap_alloc_t;

/// Determine the size of a bitmap structure required to allocate @p n blocks.
///
/// @param n The number of blocks we need to manage.
///
/// @return The size of the bitmap_alloc_t required to manage @p n blocks.
size_t bitmap_alloc_sizeof(const uint32_t n)
  HPX_INTERNAL;

/// Initialize a bitmap.
///
/// This isn't synchronized and shouldn't be used from possibly-concurrent
/// threads.
///
/// @param   bitmap The bitmap to initialize.
/// @param        n The number of blocks we need to manage.
void bitmap_alloc_init(bitmap_alloc_t *bitmap, const uint32_t n)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Allocate @p n contiguous blocks from the bitmap.
///
/// @param[in]   bitmap The bitmap to allocate from.
/// @param[in]        n The number of continuous blocks to allocate.
/// @param[out]       i The offset of the start of the allocation.
///
/// @returns LIBHPX_OK, LIBHPX_ENOMEM
int bitmap_alloc_alloc(bitmap_alloc_t *bitmap, const uint32_t from,
                       uint32_t *i)
  HPX_INTERNAL HPX_NON_NULL(1,3);

/// Free @p n contiguous blocks of memory, starting at offset @p i.
///
/// @param   bitmap The bitmap to free from.
/// @param     from The offset to start freeing from.
/// @param        n The number of blocks to free.
void bitmap_alloc_free(bitmap_alloc_t *bitmap, const uint32_t from,
                       const uint32_t n)
  HPX_INTERNAL HPX_NON_NULL(1);

#endif
