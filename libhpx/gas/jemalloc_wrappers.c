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

#include <errno.h>
#include <stdbool.h>
#include <jemalloc/jemalloc.h>
#include "libhpx/debug.h"
#include "mallctl.h"

size_t lhpx_mallctl_get_chunk_size(void) {
  size_t log2_bytes_per_chunk = 0;
  size_t sz = sizeof(log2_bytes_per_chunk);
  int e = mallctl("opt.lg_chunk", &log2_bytes_per_chunk, &sz, NULL, 0);
  if (e) {
    dbg_error("pgas: failed to read the jemalloc chunk size\n");
  }

  return 1 << log2_bytes_per_chunk;
}


void *malloc(size_t bytes) {
  return mallocx(bytes, 0);
}

void free(void *ptr) {
  if (ptr)
    dallocx(ptr, 0);
}

void *calloc(size_t nmemb, size_t size) {
  return mallocx(nmemb * size, MALLOCX_ZERO);
}

void *realloc(void *ptr, size_t size) {
  return rallocx(ptr, size, 0);
}

void *valloc(size_t size) {
  return mallocx(size, MALLOCX_ALIGN(HPX_PAGE_SIZE));
}

void *memalign(size_t boundary, size_t size) {
  return mallocx(size, MALLOCX_ALIGN(boundary));
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
  *memptr = mallocx(size, MALLOCX_ALIGN(alignment));
  return (*memptr == 0) ? ENOMEM : 0;
}

