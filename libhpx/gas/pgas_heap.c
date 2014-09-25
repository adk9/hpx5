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

#include <stdbool.h>
#include <sys/mman.h>
#include <jemalloc/jemalloc.h>
#include <libsync/sync.h>
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "pgas_heap.h"
#include "jemalloc_mallctl_wrappers.h"

static size_t get_nchunks(const size_t size, size_t bytes_per_chunk) {
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

static lhpx_bitmap_alloc_t *new_bitmap(size_t nchunks) {
  assert(nchunks <= UINT32_MAX);
  lhpx_bitmap_alloc_t *bitmap = lhpx_bitmap_alloc_new(*(uint32_t*)&nchunks);
  if (!bitmap)
    dbg_error("pgas: failed to allocate a bitmap to track free chunks.\n");
  return bitmap;
}


static void *map_heap(const size_t bytes) {
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

int lhpx_pgas_heap_init(lhpx_pgas_heap_t *heap, const size_t size) {
  assert(heap);
  assert(size);

  sync_store(&heap->csbrk, 0, SYNC_RELEASE);

  heap->bytes_per_chunk = lhpx_jemalloc_get_chunk_size();
  dbg_log_gas("pgas: heap bytes per chunk is %lu\n", heap->bytes_per_chunk);

  heap->nchunks = get_nchunks(size, heap->bytes_per_chunk);
  dbg_log_gas("pgas: heap nchunks is %lu\n", heap->nchunks);

  heap->chunks = new_bitmap(heap->nchunks);
  dbg_log_gas("pgas: allocated chunk bitmap to manage %lu chunks.\n", heap->nchunks);

  heap->nbytes = heap->nchunks * heap->bytes_per_chunk;
  if (heap->nbytes != size)
    dbg_log_gas("pgas: heap allocation of %lu requested, adjusted to %lu\n",
                size, heap->nbytes);

  dbg_log_gas("pgas: allocating %lu bytes for the shared heap.\n", heap->nbytes);
  heap->bytes = map_heap(heap->nbytes);
  dbg_log_gas("pgas: allocated heap.\n");

  return LIBHPX_OK;
}

void lhpx_pgas_heap_fini(lhpx_pgas_heap_t *heap) {
  if (!heap)
    return;

  if (heap->chunks)
    lhpx_bitmap_alloc_delete(heap->chunks);

  if (heap->bytes) {
    int e = munmap(heap->bytes, heap->nbytes);
    if (e)
      dbg_error("pgas: failed to munmap the heap.\n");
  }
}
