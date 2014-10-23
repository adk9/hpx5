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
#include <string.h>
#include <jemalloc/jemalloc.h>
#include <libsync/sync.h>
#include <hpx/builtins.h>
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/transport.h"
#include "../mallctl.h"
#include "bitmap.h"
#include "heap.h"
#include "pgas.h"
#ifdef CRAY_HUGE_HACK
#include <hugetlbfs.h>
#endif


/// This operates as an atomic fetch and add, with the added caveat that the
/// "fetched" value will be aligned to an @p align alignment.
///
/// @param            p The pointer to update.
/// @param            n The integer to add.
/// @param        align The alignment we need.
///
/// @returns The @p align-aligned fetched value.
static uint32_t _fetch_align_and_add(volatile uint64_t *p, uint32_t n,
                                     uint32_t align) {
  uint64_t fetch = 0;
  uint64_t add = 0;
  do {
    fetch = sync_load(p, SYNC_ACQUIRE);
    uint64_t r = (align - (fetch % align)) % align;
    add = fetch + r;
  } while (!sync_cas(p, fetch, add + n, SYNC_ACQ_REL, SYNC_RELAXED));

  return add;
}


static void *_heap_chunk_alloc_cyclic(heap_t *heap, size_t bytes, size_t align)
{
  assert(bytes % heap->bytes_per_chunk == 0);
  assert(align % heap->bytes_per_chunk == 0);

  uint64_t   bits = bytes / heap->bytes_per_chunk;
  uint64_t balign = align / heap->bytes_per_chunk;
  assert(bits < UINT32_MAX);
  assert(balign < UINT32_MAX);

  uint32_t bit = 0;
  if (bitmap_reserve(heap->chunks, bits, balign, &bit))
    goto oom;

  uint64_t offset = bit * heap->bytes_per_chunk;
  assert(offset % align == 0);
  heap_set_csbrk(heap, offset + bytes);
  return heap_offset_to_lva(heap, offset);

 oom:
  dbg_error("out-of-memory detected\n");
  return NULL;
}


/// The static chunk allocator callback that we give to jemalloc arenas that
/// manage the cyclic portion of our global heap.
///
/// When the cyclic arena needs to service an allocation request that it does
/// not currently have enough correctly aligned space to deal with, it will use
/// this callback, which will get a cyclic chunk from the heap.
///
/// @note This callback is only necessary to pick up the global heap pointer,
///       because the jemalloc callback registration doesn't allow us to
///       register user data to be passed back to us.
///
/// @note I do not know what the @p arena index is useful for---Luke.
///
/// @param[in]  UNUSED1 A requested address for realloc.
/// @param[in]     size The number of bytes we need to allocate.
/// @param[in]    align The alignment that is being requested.
/// @param[in/out] zero Set to zero if the chunk is pre-zeroed.
/// @param[in]  UNUSED2 The index of the arena making this allocation request.
///
/// @returns The base pointer of the newly allocated chunk.
static void *_chunk_alloc_cyclic(void *UNUSED1, size_t size, size_t align,
                                   bool *zero, unsigned UNUSED) {
  void *chunk = _heap_chunk_alloc_cyclic(global_heap, size, align);
  if (zero && *zero)
    memset(chunk, 0, size);
  return chunk;
}


/// The static chunk de-allocator callback that we give to jemalloc arenas that
/// manage the cyclic region of our global heap.
///
/// When a jemalloc arena wants to de-allocate a previously-allocated chunk for
/// any reason, it will use its currently configured chunk_dalloc_t callback to
/// do so. This is typically munmap(), however for memory corresponding to the
/// global address space we want to return the memory to our heap. This callback
/// performs that operation.
///
/// @note This callback is only necessary to pick up the global heap pointer,
///       because the jemalloc callback registration doesn't allows us to
///       register user data to be passed back to us.
///
/// @note I do not know what use the @p arena index is---Luke.
///
/// @note I do not know what the return value is used for---Luke.
///
/// @param   chunk The base address of the chunk to de-allocate, must match an
///                address returned from _chunk_alloc().
/// @param    size The number of bytes that were originally requested, must
///                match the number of bytes provided to the _chunk_alloc()
///                request associated with @p chunk.
/// @param   arena The index of the arena making the call to _chunk_dalloc().
///
/// @returns UNKNOWN---Luke.
static bool _chunk_dalloc_cyclic(void *chunk, size_t size, unsigned UNUSED) {
  return heap_chunk_dalloc(global_heap, chunk, size);
}


///
static bitmap_t *_new_bitmap(size_t nchunks) {
  assert(nchunks <= UINT32_MAX);
  bitmap_t *bitmap = bitmap_new((uint32_t)nchunks);
  if (!bitmap)
    dbg_error("failed to allocate a bitmap to track free chunks.\n");
  return bitmap;
}


int heap_init(heap_t *heap, const size_t size, bool init_cyclic) {
  assert(heap);
  assert(size);

  sync_store(&heap->csbrk, 0, SYNC_RELEASE);

  heap->bytes_per_chunk = mallctl_get_chunk_size();
  dbg_log_gas("heap bytes per chunk is %lu\n", heap->bytes_per_chunk);

  // align size to bytes-per-chunk boundary
  heap->nbytes = size - (size % heap->bytes_per_chunk);
  dbg_log_gas("heap nbytes is aligned as %lu\n", heap->nbytes);

  heap->nchunks = ceil_div_64(heap->nbytes, heap->bytes_per_chunk);
  dbg_log_gas("heap nchunks is %lu\n", heap->nchunks);

  // use one extra chunk to deal with alignment
  heap->raw_nchunks = heap->nchunks + 1;
  heap->raw_nbytes  = heap->raw_nchunks * heap->bytes_per_chunk;
  heap->raw_base    = malloc(heap->raw_nbytes);
  if (!heap->raw_base) {
    dbg_error("could not allocate %lu bytes for the global heap\n",
              heap->raw_nbytes);
    return LIBHPX_ENOMEM;
  }
  dbg_log_gas("allocated %lu bytes for the global heap\n", heap->raw_nbytes);

  // adjust stored base based on alignment requirements
  const size_t r = ((uintptr_t)heap->raw_base % heap->bytes_per_chunk);
  const size_t l = (heap->bytes_per_chunk - r) % heap->bytes_per_chunk;
  heap->base = heap->raw_base + l;
  dbg_log_gas("%lu-byte heap reserved at %p\n", heap->nbytes, heap->base);

  assert((uintptr_t)heap->base % heap->bytes_per_chunk == 0);
  assert(heap->base + heap->nbytes <= heap->raw_base + heap->raw_nbytes);

  heap->chunks = _new_bitmap(heap->nchunks);
  dbg_log_gas("allocated chunk bitmap to manage %lu chunks.\n", heap->nchunks);

  if (init_cyclic) {
    heap->cyclic_arena = mallctl_create_arena(_chunk_alloc_cyclic,
                                              _chunk_dalloc_cyclic);
    dbg_log_gas("allocated the arena to manage cyclic allocations.\n");
  }
  else {
    heap->cyclic_arena = UINT_MAX;
  }

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
    free(heap->raw_base);
  }
}


void *heap_chunk_alloc(heap_t *heap, size_t bytes, size_t align) {
  assert(bytes % heap->bytes_per_chunk == 0);
  assert(align % heap->bytes_per_chunk == 0);

  uint64_t   bits = bytes / heap->bytes_per_chunk;
  uint64_t balign = align / heap->bytes_per_chunk;
  assert(bits < UINT32_MAX);
  assert(balign < UINT32_MAX);

  uint32_t bit = 0;
  if (bitmap_rreserve(heap->chunks, bits, balign, &bit))
    goto oom;

  uint64_t offset = bit * heap->bytes_per_chunk;
  assert(offset % align == 0);

  if (offset < heap->csbrk)
    goto oom;

  return heap->base + offset;

 oom:
  dbg_error("out-of-memory detected\n");
  return NULL;
}


bool heap_chunk_dalloc(heap_t *heap, void *chunk, size_t size) {
  const uint64_t offset = (char*)chunk - heap->base;
  assert(offset % heap->bytes_per_chunk == 0);
  assert(size % heap->bytes_per_chunk == 0);

  const uint64_t    bit = offset / heap->bytes_per_chunk;
  const uint64_t  nbits = size / heap->bytes_per_chunk;

  bitmap_release(heap->chunks, bit, nbits);
  return true;
}


int heap_bind_transport(heap_t *heap, transport_class_t *transport) {
  heap->transport = transport;
  return transport->pin(transport, heap->base, heap->nbytes);
}


bool heap_contains_lva(const heap_t *heap, const void *lva) {
  const ptrdiff_t d = (char*)lva - heap->base;
  return (0 <= d && d < heap->nbytes);
}


uint64_t heap_lva_to_offset(const heap_t *heap, const void *lva) {
  DEBUG_IF (!heap_contains_lva(heap, lva)) {
    dbg_error("local virtual address %p is not in the global heap\n", lva);
  }
  return ((char*)lva - heap->base);
}


void *heap_offset_to_lva(const heap_t *heap, uint64_t offset) {
  DEBUG_IF (heap->nbytes < offset) {
    dbg_error("offset %lu out of range (0,%lu)\n", offset, heap->nbytes);
  }

  return heap->base + offset;
}


uint64_t heap_alloc_cyclic(heap_t *heap, size_t n, uint32_t bsize) {
  assert(heap->cyclic_arena < UINT32_MAX);

  // Figure out how many blocks per node that we need, and then allocate that
  // much cyclic space from the heap.
  uint64_t blocks = ceil_div_64(n, here->ranks);
  uint32_t  align = ceil_log2_32(bsize);
  assert(align < 32);
  uint32_t padded = 1u << align;
  int       flags = MALLOCX_LG_ALIGN(align) | MALLOCX_ARENA(heap->cyclic_arena);
  void      *base = libhpx_global_mallocx(blocks * padded, flags);
  if (!base)
    dbg_error("failed cyclic allocation\n");
  return heap_lva_to_offset(heap, base);
}


void heap_free_cyclic(heap_t *heap, uint64_t offset) {
  void *lva = heap_offset_to_lva(heap, offset);
  int flags = MALLOCX_ARENA(heap->cyclic_arena);
  libhpx_global_dallocx(lva, flags);
}


bool heap_offset_is_cyclic(const heap_t *heap, uint64_t offset) {
  if (offset >= heap->nbytes) {
    dbg_log_gas("offset %lu is not in the heap\n", offset);
    return false;
  }

  return (offset < heap->csbrk);
}


static bool _chunks_are_used(const heap_t *heap, uint64_t offset, size_t n) {
  uint32_t from = offset / heap->bytes_per_chunk;
  uint32_t to = (offset + n) / heap->bytes_per_chunk + 1;
  return bitmap_is_set(heap->chunks, from, to - from);
}


uint64_t heap_csbrk(heap_t *heap, size_t n, uint32_t bsize) {
  // need to allocate properly aligned offset
  uint32_t padded = (uint32_t)1 << ceil_log2_32(bsize);
  size_t    bytes = n * padded;
  uint64_t offset = _fetch_align_and_add(&heap->csbrk, bytes, padded);

  if (offset + bytes > heap->nbytes)
    goto oom;

  if (_chunks_are_used(heap, offset, bytes))
    goto oom;

  return offset;

oom:
  dbg_error("\n"
            "out-of-memory detected during csbrk allocation\n"
            "\t-global heap size: %lu bytes\n"
            "\t-previous cyclic allocation total: %lu bytes\n"
            "\t-current allocation request: %lu bytes\n",
            heap->nbytes, offset, bytes);

  return 0;
}


uint64_t heap_get_csbrk(const heap_t *heap) {
  return sync_load(&heap->csbrk, SYNC_ACQUIRE);
}


int heap_set_csbrk(heap_t *heap, uint64_t offset) {
  // csbrk is monotonically increasing, so if we see a value in the csbrk field
  // larger than the new offset, it means that this is happening out of order
  uint64_t old = sync_load(&heap->csbrk, SYNC_RELAXED);
  if (old < offset)
    sync_cas(&heap->csbrk, old, offset, SYNC_RELAXED, SYNC_RELAXED);
  return (_chunks_are_used(heap, old, offset)) ? HPX_ERROR : HPX_SUCCESS;
}
