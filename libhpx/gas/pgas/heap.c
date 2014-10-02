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
# include "config.h"
#endif

#include <inttypes.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <jemalloc/jemalloc.h>
#include <libsync/sync.h>
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/transport.h"
#include "../mallctl.h"
#include "heap.h"

static size_t _get_nchunks(const size_t size, size_t bytes_per_chunk) {
  size_t nchunks = size / bytes_per_chunk;
  if (nchunks == 0) {
    dbg_log_gas("pgas: must have at least %lu bytes in the shared heap\n",
                bytes_per_chunk);
    nchunks = 1;
  }

  if (nchunks == 1)
    dbg_log_gas("pgas: disabling support for cyclic allocation\n");
  return nchunks;
}

static bitmap_t *_new_bitmap(size_t nchunks) {
  assert(nchunks <= UINT32_MAX);
  bitmap_t *bitmap = bitmap_new((uint32_t)nchunks);
  if (!bitmap)
    dbg_error("pgas: failed to allocate a bitmap to track free chunks.\n");
  return bitmap;
}

static void *_map_heap(const size_t bytes) {
  const int prot = PROT_READ | PROT_WRITE;
  const int flags = MAP_ANON | MAP_PRIVATE | MAP_NORESERVE;
  void *heap = mmap(NULL, bytes, prot, flags, -1, 0);
  if (!heap) {
    dbg_error("pgas: failed to mmap %lu bytes for the shared heap\n", bytes);
  }
  else {
    dbg_log_gas("pgas: mmaped %lu bytes for the shared heap\n", bytes);
  }
  return heap;
}

/// Compute the number of chunks required to satisfy the @p size.
static uint32_t _chunks(const size_t size, const size_t bytes_per_chunk) {
  uint32_t chunks = size / bytes_per_chunk;
  chunks += (size % bytes_per_chunk) ? 1 : 0;
  return chunks;
}

int heap_init(heap_t *heap, const size_t size) {
  assert(heap);
  assert(size);

  sync_store(&heap->csbrk, 0, SYNC_RELEASE);

  heap->bytes_per_chunk = mallctl_get_chunk_size();
  dbg_log_gas("pgas: heap bytes per chunk is %lu\n", heap->bytes_per_chunk);

  heap->raw_nchunks = _get_nchunks(size, heap->bytes_per_chunk);
  dbg_log_gas("pgas: heap raw nchunks is %lu\n", heap->raw_nchunks);

  heap->raw_nbytes = heap->raw_nchunks * heap->bytes_per_chunk;
  if (heap->raw_nbytes != size)
    dbg_log_gas("pgas: heap allocation of %lu requested, adjusted to %lu\n",
                size, heap->raw_nbytes);

  dbg_log_gas("pgas: allocating %lu bytes for the shared heap.\n",
              heap->raw_nbytes);
  heap->raw_base = _map_heap(heap->raw_nbytes);

  // Adjust base, nbytes, raw, based on alignment requirements.
  const size_t r = heap->bytes_per_chunk - ((uintptr_t)heap->raw_base %
                                            heap->bytes_per_chunk);

  heap->base = heap->raw_base + r;
  heap->nbytes = heap->raw_nbytes - r;
  heap->nchunks = heap->raw_nchunks - (r != 0) ? 1 : 0;

  heap->chunks = _new_bitmap(heap->nchunks);
  dbg_log_gas("pgas: allocated chunk bitmap to manage %lu chunks.\n",
              heap->nchunks);

  dbg_log_gas("pgas: allocated heap.\n");

  return LIBHPX_OK;
}

void heap_fini(heap_t *heap) {
  if (!heap)
    return;

  if (heap->chunks)
    bitmap_delete(heap->chunks);

  if (heap->raw_base) {
    if (heap->transport)
      heap->transport->unpin(heap->transport, heap->base, heap->nbytes);

    int e = munmap(heap->raw_base, heap->raw_nbytes);
    if (e)
      dbg_error("pgas: failed to munmap the heap.\n");
  }
}

void *heap_chunk_alloc(heap_t *heap, size_t size, size_t alignment, bool *zero,
                       unsigned arena) {
  assert(arena == mallctl_thread_get_arena());
  const uint32_t blocks = _chunks(size, heap->bytes_per_chunk);
  const uint32_t align = _chunks(alignment, heap->bytes_per_chunk);
  uint32_t offset = 0;
  int e = bitmap_reserve(heap->chunks, blocks, align, &offset);
  dbg_check(e, "pgas: failed to allocate a chunk size %"PRIu32
            " align %"PRIu32"\n", blocks, align);

  if (zero)
    *zero = false;

  char *chunk = heap->base + offset * heap->bytes_per_chunk;
  assert((uintptr_t)chunk % alignment == 0);
  return chunk;
}

bool heap_chunk_dalloc(heap_t *heap, void *chunk, size_t size, unsigned arena) {
  const uint32_t offset = (char*)chunk - heap->base;
  assert(offset % heap->bytes_per_chunk == 0);
  const uint32_t i = _chunks(offset, heap->bytes_per_chunk);
  const uint32_t n = _chunks(size, heap->bytes_per_chunk);
  bitmap_release(heap->chunks, i, n);
  return true;
}

bool heap_contains(heap_t *heap, void *addr) {
  const ptrdiff_t d = (char*)addr - heap->base;
  return (0 <= d && d < heap->nbytes);
}

bool heap_is_cyclic(heap_t *heap, void *addr) {
  assert(heap_contains(heap, addr));
  if (HEAP_USE_CYCLIC_CSBRK_BARRIER)
    return (char*)addr >= heap->base + heap->nbytes - heap->csbrk;

  // see if the chunk is allocated
  const uint32_t chunk = heap_offset_of(heap, addr) / heap->bytes_per_chunk;
  return bitmap_is_set(heap->chunks, chunk);
}

void heap_bind_transport(heap_t *heap, transport_class_t *transport) {
  transport->pin(transport, heap->base, heap->nbytes);
  heap->transport = transport;
}

uint64_t heap_offset_of(heap_t *heap, void *addr) {
  DEBUG_IF (!heap_contains(heap, addr)) {
    dbg_error("local virtual address %p is not in the global heap\n", addr);
  }
  return ((char*)addr - heap->base);
}
