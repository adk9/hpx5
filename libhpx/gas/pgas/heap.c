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
#include <hpx/builtins.h>
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/transport.h"
#include "../bitmap.h"
#include "../mallctl.h"
#include "heap.h"
#ifdef CRAY_HUGE_HACK
#include <hugetlbfs.h>
#endif

static bitmap_t *_new_bitmap(size_t nchunks) {
  assert(nchunks <= UINT32_MAX);
  bitmap_t *bitmap = bitmap_new((uint32_t)nchunks);
  if (!bitmap)
    dbg_error("failed to allocate a bitmap to track free chunks.\n");
  return bitmap;
}

static void *_map_heap(const size_t bytes) {
  const int prot = PROT_READ | PROT_WRITE;
  const int flags = MAP_ANON | MAP_PRIVATE | MAP_NORESERVE;
#ifdef CRAY_HUGE_HACK
  void *heap = get_huge_pages((bytes + gethugepagesize() - 1) / gethugepagesize() * gethugepagesize(), GHP_DEFAULT);
#else
  void *heap = mmap(NULL, bytes, prot, flags, -1, 0);
#endif
  if (!heap) {
    dbg_error("failed to mmap %lu bytes for the shared heap\n", bytes);
  }
  else {
    dbg_log_gas("mmaped %lu bytes for the shared heap\n", bytes);
  }
  return heap;
}

int heap_init(heap_t *heap, const size_t size) {
  assert(heap);
  assert(size);

  sync_store(&heap->csbrk, 0, SYNC_RELEASE);

  heap->bytes_per_chunk = mallctl_get_chunk_size();
  dbg_log_gas("heap bytes per chunk is %lu\n", heap->bytes_per_chunk);

  heap->nbytes = size;
  heap->nchunks = ceil_div_64(size, heap->bytes_per_chunk);
  dbg_log_gas("heap nchunks is %lu\n", heap->nchunks);

  // use one extra chunk to deal with alignment
  heap->raw_nchunks = heap->nchunks + 1;
  heap->raw_nbytes = heap->raw_nchunks * heap->bytes_per_chunk;
  heap->raw_base = _map_heap(heap->raw_nbytes);

  // adjust stored base based on alignment requirements
  const size_t r = ((uintptr_t)heap->raw_base % heap->bytes_per_chunk);
  const size_t l = heap->bytes_per_chunk - r;
  heap->base = heap->raw_base + l;
  dbg_log_gas("%lu-byte heap reserved at %p\n", heap->nbytes, heap->base);

  assert((uintptr_t)heap->base % heap->bytes_per_chunk == 0);
  assert(heap->base + heap->nbytes <= heap->raw_base + heap->raw_nbytes);

  heap->chunks = _new_bitmap(heap->nchunks);
  dbg_log_gas("allocated chunk bitmap to manage %lu chunks.\n", heap->nchunks);
  dbg_log_gas("allocated heap.\n");

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

#ifdef CRAY_HUGE_HACK
    free_huge_pages(heap->raw_base);
#else
    int e = munmap(heap->raw_base, heap->raw_nbytes);
    if (e)
      dbg_error("pgas: failed to munmap the heap.\n");
#endif
  }
}

void *heap_chunk_alloc(heap_t *heap, size_t size, size_t alignment) {
  const uint32_t blocks = ceil_div_64(size, heap->bytes_per_chunk);
  const uint32_t align  = ceil_div_64(alignment, heap->bytes_per_chunk);
  uint32_t chunk_offset = 0;
  int e = bitmap_reserve(heap->chunks, blocks, align, &chunk_offset);
  dbg_check(e, "pgas: failed to allocate a chunk size %"PRIu32
            " align %"PRIu32"\n", blocks, align);

  const uint64_t heap_offset = chunk_offset * heap->bytes_per_chunk;
  const uint64_t cyclic_offset = heap->nbytes - sync_load(&heap->csbrk,
                                                          SYNC_RELAXED);
  if (cyclic_offset < heap_offset) {
    dbg_error("\n"
              "out-of-memory detected\n"
              "\t-gas_alloc is using %lu bytes\n"
              "\t-gas_global_alloc is using %lu bytes per locality\n",
              heap_offset, heap->csbrk);
  }

  char *chunk = heap->base + heap_offset;
  const uint64_t actual_alignment = (uintptr_t)chunk % alignment;
  if (actual_alignment != 0) {
    dbg_error("expected chunk with alignment %lu, off by %lu\n", alignment,
              actual_alignment);
  }

  return chunk;
}

bool heap_chunk_dalloc(heap_t *heap, void *chunk, size_t size) {
  const uint32_t offset = (char*)chunk - heap->base;
  assert(offset % heap->bytes_per_chunk == 0);
  const uint32_t i = offset / heap->bytes_per_chunk;
  const uint32_t n = ceil_div_64(size, heap->bytes_per_chunk);
  bitmap_release(heap->chunks, i, n);
  return true;
}

bool heap_contains(heap_t *heap, void *addr) {
  const ptrdiff_t d = (char*)addr - heap->base;
  return (0 <= d && d < heap->nbytes);
}

int heap_bind_transport(heap_t *heap, transport_class_t *transport) {
  heap->transport = transport;
  return transport->pin(transport, heap->base, heap->nbytes);
}

uint64_t heap_lva_to_offset(heap_t *heap, void *lva) {
  DEBUG_IF (!heap_contains(heap, lva)) {
    dbg_error("local virtual address %p is not in the global heap\n", lva);
  }
  return ((char*)lva - heap->base);
}

void *heap_offset_to_lva(heap_t *heap, uint64_t offset) {
  DEBUG_IF (heap->nbytes < offset) {
    dbg_error("offset %lu out of range (0,%lu)\n", offset, heap->nbytes);
  }

  return heap->base + offset;
}

bool heap_offset_is_cyclic(heap_t *heap, uint64_t heap_offset) {
  if (!heap_offset_inbounds(heap, heap_offset)) {
    dbg_log_gas("offset %lu is not in the heap\n", heap_offset);
    return false;
  }

  if (HEAP_USE_CYCLIC_CSBRK_BARRIER)
    return heap_offset > (heap->nbytes - heap->csbrk);

  // see if the chunk is allocated
  const uint32_t chunk = heap_offset / heap->bytes_per_chunk;
  const bool acyclic = bitmap_is_set(heap->chunks, chunk);
  return !acyclic;
}

int _check_heap_offsets(heap_t *heap, uint64_t base, uint64_t size) {
  // check to see if any of this allocation is reserved in the bitmap
  const uint64_t from = base / heap->bytes_per_chunk;
  const uint64_t to = (base + size) / heap->bytes_per_chunk;

  for (uint64_t i = from, e = to; i < e; ++i) {
    if (bitmap_is_set(heap->chunks, i)) {
      dbg_error("out-of-memory detected, csbrk allocation collided with the "
                "gas_alloc heap \n");
    }
  }

  return LIBHPX_OK;
}

size_t heap_csbrk(heap_t *heap, size_t n, uint32_t bsize) {
  // need to allocate properly aligned offset
  const size_t bytes = n * bsize;
  uint64_t old = 0;
  uint64_t new = 0;
  do {
    old = sync_load(&heap->csbrk, SYNC_RELAXED);
    const size_t end = old + bytes;
    const size_t offset = heap->nbytes - end;
    const uint32_t r = offset % bsize;
    new = end + r;
  } while (!sync_cas(&heap->csbrk, old, new, SYNC_ACQ_REL, SYNC_RELAXED));

  if (new >= heap->nbytes)
    dbg_error("\n"
              "out-of-memory detected during csbrk allocation\n"
              "\t-global heap size: %lu bytes\n"
              "\t-previous cyclic allocation total: %lu bytes\n"
              "\t-current allocation request: %lu bytes\n",
              heap->nbytes, old, bytes);

  const uint64_t heap_offset = (heap->nbytes - new);
  assert(heap_offset % bsize == 0);
  assert(heap_offset_inbounds(heap, heap_offset));
  _check_heap_offsets(heap, heap_offset, bytes);
  return heap_offset;
}

bool heap_offset_inbounds(heap_t *heap, uint64_t heap_offset) {
  return (heap_offset < heap->nbytes);
}

int heap_set_csbrk(heap_t *heap, uint64_t heap_offset) {
  const uint64_t new = heap->nbytes - heap_offset;
  const size_t old = sync_swap(&heap->csbrk, new, SYNC_ACQ_REL);
  if (new < old)
    dbg_error("csbrk should be monotonically increasing");

  return _check_heap_offsets(heap, heap_offset, new - old);
}
