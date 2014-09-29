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

#include <limits.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <jemalloc/jemalloc.h>
#include <hpx/hpx.h>
#include "libhpx/libhpx.h"
#include "libhpx/debug.h"
#include "mallctl.h"
#include "pgas.h"
#include "pgas_heap.h"

static lhpx_pgas_heap_t _heap = {
  .csbrk = 0,
  .bytes_per_chunk = 0,
  .nchunks = 0,
  .chunks = NULL,
  .nbytes = 0,
  .bytes = NULL
};

static __thread unsigned _pvt_arena = UINT_MAX;

static uint32_t _chunks(size_t size) {
  uint32_t chunks = size / _heap.bytes_per_chunk;
  chunks += (size % _heap.bytes_per_chunk) ? 1 : 0;
  return chunks;
}

static void *_shared_chunk_alloc(size_t size, size_t alignment, bool *zero,
                                 unsigned arena) {
  assert(arena == lhpx_mallctl_thread_get_arena());
  const uint32_t blocks = _chunks(size);
  const uint32_t align = _chunks(alignment);
  uint32_t offset = 0;
  int e = lhpx_bitmap_alloc_alloc(_heap.chunks, blocks, align, &offset);
  dbg_check(e, "pgas: failed to allocate a chunk size %"PRIu32
            " align %"PRIu32"\n", blocks, align);
  if (zero)
    *zero = false;

  char *chunk = _heap.bytes + offset * _heap.bytes_per_chunk;
  assert((uintptr_t)chunk % alignment == 0);
  return chunk;
}

static bool _shared_chunk_dalloc(void *chunk, size_t size, unsigned arena) {
  assert(arena == lhpx_mallctl_thread_get_arena());
  const uint32_t offset = (char*)chunk - _heap.bytes;
  assert(offset % _heap.bytes_per_chunk == 0);
  const uint32_t i = _chunks(offset);
  const uint32_t n = _chunks(size);
  lhpx_bitmap_alloc_free(_heap.chunks, i, n);
  return true;
}

int lhpx_pgas_init(size_t heap_size) {
  if (!lhpx_mallctl_get_lg_dirty_mult()) {
    fprintf(stderr,
            "HPX requires \"lg_dirty_mult:-1\" set in the environment variable "
            "MALLOC_CONF\n");
    hpx_abort();
  }

  return lhpx_pgas_heap_init(&_heap, heap_size);
}

int lhpx_pgas_init_worker(void) {
  if (_pvt_arena != UINT_MAX)
    return LIBHPX_OK;

  unsigned arena = lhpx_mallctl_create_arena(_shared_chunk_alloc,
                                             _shared_chunk_dalloc);
  lhpx_mallctl_thread_enable_cache();
  lhpx_mallctl_thread_flush_cache();
  _pvt_arena = lhpx_mallctl_thread_set_arena(arena);
  return LIBHPX_OK;
}

void lhpx_pgas_fini_worker(void) {

}

void lhpx_pgas_fini(void) {
  lhpx_pgas_heap_fini(&_heap);
}


void *lhpx_pgas_malloc(size_t bytes) {
  if (!bytes)
    return NULL;

  if (_pvt_arena != UINT_MAX)
    return mallocx(bytes, MALLOCX_ARENA(_pvt_arena));
  else
    return mallocx(bytes, 0);
}

void lhpx_pgas_free(void *ptr) {
  if (!ptr) return;

  if (_pvt_arena != UINT_MAX)
    dallocx(ptr, MALLOCX_ARENA(_pvt_arena));
  else
    dallocx(ptr, 0);
}

void *lhpx_pgas_calloc(size_t nmemb, size_t size) {
  if (_pvt_arena != UINT_MAX)
    return mallocx(nmemb * size, MALLOCX_ARENA(_pvt_arena) | MALLOCX_ZERO);
  else
    return mallocx(nmemb * size, MALLOCX_ZERO);
}

void *lhpx_pgas_realloc(void *ptr, size_t size) {
  if (_pvt_arena != UINT_MAX)
    return rallocx(ptr, size, MALLOCX_ARENA(_pvt_arena));
  else
    return rallocx(ptr, size, 0);
}

void *lhpx_pgas_valloc(size_t size) {
  if (_pvt_arena != UINT_MAX)
    return mallocx(size, MALLOCX_ARENA(_pvt_arena) | MALLOCX_ALIGN(HPX_PAGE_SIZE));
  else
    return mallocx(size, MALLOCX_ALIGN(HPX_PAGE_SIZE));
}

void *lhpx_pgas_memalign(size_t boundary, size_t size) {
  if (_pvt_arena != UINT_MAX)
    return mallocx(size, MALLOCX_ARENA(_pvt_arena) | MALLOCX_ALIGN(boundary));
  else
    return mallocx(size, MALLOCX_ALIGN(boundary));
}

int lhpx_pgas_posix_memalign(void **memptr, size_t alignment, size_t size) {
  if (_pvt_arena != UINT_MAX)
    *memptr = mallocx(size, MALLOCX_ARENA(_pvt_arena) | MALLOCX_ALIGN(alignment));
  else
    *memptr = mallocx(size, MALLOCX_ALIGN(alignment));

  return (*memptr == 0) ? ENOMEM : 0;
}
