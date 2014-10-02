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
#include "libhpx/locality.h"
#include "malloc.h"

void *malloc(size_t bytes) {
  return (here && here->gas) ? local_malloc(bytes)
                             : default_malloc(bytes);
}

void free(void *ptr) {
  if (here && here->gas)
    local_free(ptr);
  else
    default_free(ptr);
}

void *calloc(size_t nmemb, size_t size) {
  return (here && here->gas) ? local_calloc(nmemb, size)
                             : default_calloc(nmemb, size);
}

void *realloc(void *ptr, size_t size) {
  return (here && here->gas) ? local_realloc(ptr, size)
                             : default_realloc(ptr, size);
}

void *valloc(size_t size) {
  return (here && here->gas) ? local_valloc(size)
                             : default_valloc(size);
}

void *memalign(size_t boundary, size_t size) {
  return (here && here->gas) ? local_memalign(boundary, size)
                             : default_memalign(boundary, size);
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
  return (here && here->gas) ? local_posix_memalign(memptr, alignment, size)
                             : default_posix_memalign(memptr, alignment, size);
}

void *arena_malloc(unsigned arena, size_t bytes) {
  assert(arena != UINT_MAX);

  const int flags = MALLOCX_ARENA(arena);
  return (bytes) ? libhpx_mallocx(bytes, flags)
                 : NULL;
}

void arena_free(unsigned arena, void *ptr) {
  assert(arena != UINT_MAX);

  if (ptr) {
    const int flags = MALLOCX_ARENA(arena);
    libhpx_dallocx(ptr, flags);
  }
}

void *arena_calloc(unsigned arena, size_t nmemb, size_t size) {
  assert(arena != UINT_MAX);

  const int flags = MALLOCX_ARENA(arena) | MALLOCX_ZERO;
  return (nmemb && size) ? libhpx_mallocx(nmemb * size, flags )
                         : NULL;
}

void *arena_realloc(unsigned arena, void *ptr, size_t size) {
  assert(arena != UINT_MAX);

  const int flags = MALLOCX_ARENA(arena);
  return (ptr) ? libhpx_rallocx(ptr, size, flags)
               : arena_malloc(arena, size);
}

void *arena_valloc(unsigned arena, size_t size) {
  assert(arena != UINT_MAX);
  return arena_memalign(arena, HPX_PAGE_SIZE, size);
}

void *arena_memalign(unsigned arena, size_t boundary, size_t size) {
  assert(arena != UINT_MAX);

  const int flags = MALLOCX_ARENA(arena) | MALLOCX_ALIGN(boundary);
  return libhpx_mallocx(size, flags);
}

int arena_posix_memalign(unsigned arena, void **memptr, size_t alignment,
                         size_t size) {
  assert(arena != UINT_MAX);

  if (!size || !alignment) {
    *memptr = NULL;
    return 0;
  }

  const int flags = MALLOCX_ARENA(arena) | MALLOCX_ALIGN(alignment);
  *memptr = libhpx_mallocx(size, flags);
  return (*memptr == 0) ? ENOMEM : 0;
}

void *default_malloc(size_t bytes) {
  const int flags = 0;
  return (bytes) ? libhpx_mallocx(bytes, flags)
                 : NULL;
}

void default_free(void *ptr) {
  if (ptr) {
    const int flags = 0;
    libhpx_dallocx(ptr, flags);
  }
}

void *default_calloc(size_t nmemb, size_t size) {
  const int flags = MALLOCX_ZERO;
  return (nmemb && size) ? libhpx_mallocx(nmemb * size, flags )
                         : NULL;
}

void *default_realloc(void *ptr, size_t size) {
  const int flags = 0;
  return (ptr) ? libhpx_rallocx(ptr, size, flags)
               : default_malloc(size);
}

void *default_valloc(size_t size) {
  return default_memalign(HPX_PAGE_SIZE, size);
}

void *default_memalign(size_t boundary, size_t size) {
  const int flags = MALLOCX_ALIGN(boundary);
  return libhpx_mallocx(size, flags);
}

int default_posix_memalign(void **memptr, size_t alignment, size_t size) {
  if (!size || !alignment) {
    *memptr = NULL;
    return 0;
  }

  const int flags = MALLOCX_ALIGN(alignment);
  *memptr = libhpx_mallocx(size, flags);
  return (*memptr == 0) ? ENOMEM : 0;
}
