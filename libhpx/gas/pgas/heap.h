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
#include "bitmap.h"

#define HEAP_USE_CYCLIC_CSBRK_BARRIER 0

struct transport_class;

typedef struct heap {
  volatile size_t             csbrk;
  size_t            bytes_per_chunk;
  size_t                raw_nchunks;
  size_t                    nchunks;
  bitmap_t                  *chunks;
  size_t                     nbytes;
  char                        *base;
  size_t                 raw_nbytes;
  char                    *raw_base;
  struct transport_class *transport;
} heap_t;

int heap_init(heap_t *heap, size_t size);
void heap_fini(heap_t *heap);

void *heap_chunk_alloc(heap_t *heap, size_t size, size_t alignment, bool *zero,
                       unsigned arena)
  HPX_INTERNAL;

bool heap_chunk_dalloc(heap_t *heap, void *chunk, size_t size, unsigned arena)
  HPX_INTERNAL;

/// This operation binds the heap to a network.
///
/// Some transports need to know about the heap in order to perform network
/// operations to and from it. In particular, Photon needs to register the
/// heap's [base, base + nbytes) range.
///
/// @param        heap The heap in question---must be initialized.
/// @param   transport The transport to bind.
///
/// @returns TRUE if the transport requires mallctl_disable_dirty_page_purge().
int heap_bind_transport(heap_t *heap, struct transport_class *transport)
  HPX_INTERNAL HPX_NON_NULL(1, 2);

/// Check to see if the heap contains the given, local virtual address.
///
bool heap_contains(heap_t *heap, void *addr)
  HPX_INTERNAL;

/// Compute the relative heap_offset for this address.
///
/// This does not check to see if the address is in range. Users should either
/// check the returned value with heap_offset_inbounds(), or check beforehand
/// with heap_contains().
uint64_t heap_offset_of(heap_t *heap, void *addr)
  HPX_INTERNAL;


/// Check to see if the given offset is cyclic.
///
/// This will verify that the @p heap_offset is in the heap, out-of-bounds
/// offsets result in false.
///
/// @param           heap The heap to check in.
/// @param    heap_offset The heap-relative offset to check.
///
/// @returns TRUE if the offset is a cyclic offset, FALSE otherwise.
bool heap_offset_is_cyclic(heap_t *heap, uint64_t heap_offset)
  HPX_INTERNAL;


/// Convert a relative heap offset into a local virtual address.
///
/// Calling this function with an out-of-bound heap offset will result in
/// undefined behavior.
void *heap_offset_to_local(heap_t *heap, uint64_t heap_offset)
  HPX_INTERNAL;


/// Allocate a cyclic array of blocks.
///
/// @param           heap The heap from which to allocate.
/// @param              n The number of blocks per locality to allocate.
/// @param  aligned_bsize The base 2 alignment of the block size.
///
/// @returns the base offset of the new allocation---csbrk == heap->nbytes - offset
size_t heap_csbrk(heap_t *heap, size_t n, uint32_t aligned)
  HPX_INTERNAL;


/// Check to make sure a heap offset is actually in the heap.
bool heap_offset_inbounds(heap_t *heap, uint64_t heap_offset);


/// Check to make sure that a range of offsets is in the heap.
bool heap_range_inbounds(heap_t *heap, uint64_t start, int64_t length);

#endif
