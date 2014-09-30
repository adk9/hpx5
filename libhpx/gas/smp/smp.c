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
#include "smp.h"

void *smp_malloc(size_t bytes) {
  return mallocx(bytes, 0);
}

void smp_free(void *ptr) {
  if (ptr)
    dallocx(ptr, 0);
}

void *smp_calloc(size_t nmemb, size_t size) {
  return mallocx(nmemb * size, MALLOCX_ZERO);
}

void *smp_realloc(void *ptr, size_t size) {
  return rallocx(ptr, size, 0);
}

void *smp_valloc(size_t size) {
  return mallocx(size, MALLOCX_ALIGN(HPX_PAGE_SIZE));
}

void *smp_memalign(size_t boundary, size_t size) {
  return mallocx(size, MALLOCX_ALIGN(boundary));
}

int smp_posix_memalign(void **memptr, size_t alignment, size_t size) {
  *memptr = mallocx(size, MALLOCX_ALIGN(alignment));
  return (*memptr == 0) ? ENOMEM : 0;
}
