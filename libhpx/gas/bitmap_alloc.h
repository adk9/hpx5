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
} lhpx_bitmap_alloc_t;

/// Determine the size of a bitmap structure required to allocate @p n blocks.
///
/// @param n The number of blocks we need to manage.
///
/// @return The size of the bitmap_alloc_t required to manage @p n blocks.
size_t lhpx_bitmap_alloc_sizeof(const uint32_t n)
  HPX_INTERNAL;

/// Initialize a bitmap.
///
/// This isn't synchronized and shouldn't be used from possibly-concurrent
/// threads.
///
/// @param   bitmap The bitmap to initialize.
/// @param        n The number of blocks we need to manage.
void lhpx_bitmap_alloc_init(lhpx_bitmap_alloc_t *bitmap, const uint32_t n)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Allocate and initialize a bitmap.
///
/// @param        n The number of blocks we need to manage.
///
/// @returns The new bitmap or NULL if there was an error.
lhpx_bitmap_alloc_t *lhpx_bitmap_alloc_new(const uint32_t n)
  HPX_INTERNAL;

/// Delete a bitmap that was previously allocated with lhpx_bitmap_alloc_new().
///
/// @param   bitmap The bitmap to free,
void lhpx_bitmap_alloc_delete(lhpx_bitmap_alloc_t *bitmap)
  HPX_INTERNAL;

/// Allocate @p n contiguous blocks from the bitmap, aligned to a @p align
/// boundary.
///
/// @param[in]   bitmap The bitmap to allocate from.
/// @param[in]        n The number of continuous blocks to allocate.
/// @param[in]    align The alignment required.
/// @param[out]       i The offset of the start of the allocation.
///
/// @returns LIBHPX_OK, LIBHPX_ENOMEM
int lhpx_bitmap_alloc_alloc(lhpx_bitmap_alloc_t *bitmap, const uint32_t n,
                            const uint32_t align, uint32_t *i)
  HPX_INTERNAL HPX_NON_NULL(1, 4);

/// Free @p n contiguous blocks of memory, starting at offset @p i.
///
/// @param   bitmap The bitmap to free from.
/// @param     from The offset to start freeing from.
/// @param        n The number of blocks to free.
void lhpx_bitmap_alloc_free(lhpx_bitmap_alloc_t *bitmap, const uint32_t from,
                            const uint32_t n)
  HPX_INTERNAL HPX_NON_NULL(1);

#endif
