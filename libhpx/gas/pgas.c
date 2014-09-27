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
#include <stdbool.h>
#include <jemalloc/jemalloc.h>
#include "libhpx/libhpx.h"
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

int lhpx_pgas_init(size_t heap_size) {
  return lhpx_pgas_heap_init(&_heap, heap_size);
}

int lhpx_pgas_init_worker() {
  return LIBHPX_EUNIMPLEMENTED;
}

void lhpx_pgas_fini_worker() {
}

void lhpx_pgas_fini(void) {
  lhpx_pgas_heap_fini(&_heap);
}


void *lhpx_pgas_malloc(size_t bytes) {
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
