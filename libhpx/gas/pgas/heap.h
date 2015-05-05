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
#ifndef LIBHPX_GAS_PGAS_HEAP_H
#define LIBHPX_GAS_PGAS_HEAP_H

/// @file libhpx/gas/pgas_heap.h
/// @brief Implementation of the global address space with a PGAS model.
///
/// This implementation is similar to, and inspired by, the PGAS heap
/// implementation for UPC.
///
/// The PGAS heap implementation allocates one large region for the symmetric
/// heap, as requested by the application programmer. This region is dynamically
/// divided into cyclic and acyclic regions. Each locality manages its acyclic
/// region with a combination of jemalloc and a simple, locking-bitmap-based
/// jemalloc chunk allocator. The cyclic region is managed via an sbrk at the
/// root locality. The regions start out opposite each other in the space, and
/// grow towards each other.
///
///   +------------------------
///   | cyclic
///   |
///   | ...
///   |
///   |
///   | acyclic
///   +------------------------
///
/// We do not currently have any way to detect intersection of the cyclic and
/// acyclic regions, because the cyclic allocations are not broadcast. The root
/// has no way of knowing how much acyclic allocation each locality has
/// performed, which it would need to know to do the check.
///
/// @todo Implement a debugging mode where cyclic allocation is broadcast, so we
///       can detect intersections and report meaningful errors. Without this,
///       intersections lead to untraceable errors.
///

#include <stddef.h>
#include <hpx/attributes.h>

#define HEAP_USE_CYCLIC_CSBRK_BARRIER 0

/// Forward declarations.
/// @{
struct bitmap;
/// @}

typedef struct heap {
  volatile uint64_t     csbrk;
  size_t      bytes_per_chunk;
  size_t              nchunks;
  struct bitmap       *chunks;
  size_t               nbytes;
  char                  *base;
  unsigned       cyclic_arena;
  uint32_t  max_block_lg_size;
} heap_t;

/// Initialize a heap to manage the specified number of bytes.
///
/// @param         heap The heap pointer to initialize.
/// @param         size The number of bytes to allocate for the heap.
/// @param  init_cyclic True if we should initialize support for cyclic
///                     allocations from this heap object.
///
/// @returns LIBHPX_OK, or LIBHPX_ENOMEM if there is a problem allocating the
///          requested heap size.
int heap_init(heap_t *heap, size_t size, bool init_cyclic)
  HPX_NON_NULL(1) HPX_INTERNAL;

/// Finalize a heap.
///
/// @param         heap The heap pointer to finalize.
void heap_fini(heap_t *heap)
  HPX_NON_NULL(1) HPX_INTERNAL;

/// Allocate a chunk of the global address space.
///
/// This satisfies requests from jemalloc's chunk allocator for global memory.
///
/// @param         heap The heap object.
/// @param         addr A "suggested" address---ignored by the heap for now.
/// @param         size The number of bytes to allocate.
/// @param        align The alignment required for the chunk.
///
/// @returns The address of the base of the allocated chunk, or NULL if we are
///          out of memory.
void *heap_chunk_alloc(heap_t *heap, void *addr, size_t size, size_t align)
  HPX_INTERNAL;

/// Release a chunk of the global address space.
///
/// This satisfies requests from jemalloc's chunk allocator to release global
/// memory.
///
/// @param         heap The heap object.
/// @param        chunk The chunk base pointer to release.
/// @param         size The number of bytes associated with the chunk.
///
/// @returns I have no idea what the return value should be used for---Luke.
bool heap_chunk_dalloc(heap_t *heap, void *chunk, size_t size)
  HPX_NON_NULL(1,2) HPX_INTERNAL;

/// Check to see if the heap contains the given local virtual address.
///
/// @param         heap The heap object.
/// @param          lva The local virtual address to test.
///
/// @returns TRUE if the @p lva is contained in the global heap.
bool heap_contains_lva(const heap_t *heap, const void *lva)
  HPX_NON_NULL(1) HPX_INTERNAL;

/// Check to see if the heap contains the given offset.
///
/// @param         heap The heap object.
/// @param       offset The offset to check.
///
/// @returns TRUE if the @p offset is contained in the global heap.
bool heap_contains_offset(const heap_t *heap, uint64_t offset)
  HPX_NON_NULL(1) HPX_INTERNAL;

/// Compute the relative heap_offset for this address.
///
/// @param         heap The heap object.
/// @param          lva The local virtual address.
///
/// @returns The absolute offset of the @p lva within the global heap.
uint64_t heap_lva_to_offset(const heap_t *heap, const void *lva)
  HPX_NON_NULL(1) HPX_INTERNAL;

/// Convert a heap offset into a local virtual address.
///
/// @param         heap The heap object.
/// @param       offset The absolute offset within the heap.
///
/// @returns The local virtual address corresponding to the offset.
void *heap_offset_to_lva(const heap_t *heap, uint64_t offset)
  HPX_NON_NULL(1) HPX_INTERNAL;

uint64_t heap_alloc_cyclic(heap_t *heap, size_t n, uint32_t bsize)
  HPX_NON_NULL(1) HPX_INTERNAL;

void heap_free_cyclic(heap_t *heap, uint64_t offset)
  HPX_NON_NULL(1) HPX_INTERNAL;

/// Check to see if the given offset is cyclic.
///
/// This will verify that the @p heap_offset is in the heap, out-of-bounds
/// offsets result in false.
///
/// @param         heap The heap to check in.
/// @param       offset The heap-relative offset to check.
///
/// @returns TRUE if the offset is a cyclic offset, FALSE otherwise.
bool heap_offset_is_cyclic(const heap_t *heap, uint64_t offset)
  HPX_NON_NULL(1) HPX_INTERNAL;

/// Get the csbrk.
///
/// @param         heap The heap.
///
/// @returns The current value of csbrk.
uint64_t heap_get_csbrk(const heap_t *heap)
  HPX_NON_NULL(1) HPX_INTERNAL;

/// Set the csbrk to correspond to the given heap_offset value.
///
/// @returns LIBHPX_OK for success, LIBHPX_ENOMEM for failure.
int heap_set_csbrk(heap_t *heap, uint64_t offset)
  HPX_NON_NULL(1) HPX_INTERNAL;

/// Get the maximum number of bits that can be used for block size in the
/// current heap.
/// @param         heap The heap to check.
///
/// @returns The number of bits that can be used for block size.
uint32_t heap_max_block_lg_size(const heap_t *heap)
  HPX_NON_NULL(1) HPX_INTERNAL;

#endif
