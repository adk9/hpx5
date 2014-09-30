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
#include <hpx/malloc.h>
#include "libhpx/gas.h"
#include "libhpx/locality.h"

// We need to sit in front of all of the memory allocation routines to implement
// the following protocol. We use jemalloc for all memory allocation. All
// standard malloc.h routines should be allocating private memory. If the gas
// has not been initialized yet, then we use jemalloc directly, otherwise we
// forward to the gas-specific local implementation.

void *malloc(size_t bytes) {
  if (!bytes)
    return NULL;

  if (!here || !here->gas)
    return hpx_mallocx(bytes, 0);

  return gas_local_malloc(here->gas, bytes);
}

void free(void *ptr) {
  if (!ptr)
    return;

  if (!here || !here->btt)
    hpx_dallocx(ptr, 0);
  else
    gas_local_free(here->gas, ptr);
}


void *calloc(size_t nmemb, size_t size) {
  if (!nmemb || !size)
    return NULL;

  if (!here || !here->btt)
    return hpx_mallocx(nmemb * size, MALLOCX_ZERO);

  return gas_local_calloc(here->gas, nmemb, size);
}

void *realloc(void *ptr, size_t size) {
  if (!ptr)
    return malloc(size);

  if (!here || !here->btt)
    return hpx_rallocx(ptr, size, 0);

  return gas_local_realloc(here->gas, ptr, size);
}

void *valloc(size_t size) {
  if (!size)
    return NULL;

  if (!here || !here->btt)
    return hpx_mallocx(size, MALLOCX_ALIGN(HPX_PAGE_SIZE));

  return gas_local_valloc(here->gas, size);
}

void *memalign(size_t boundary, size_t size) {
  if (!size || !boundary)
    return NULL;

  if (!here || !here->btt)
    return hpx_mallocx(size, MALLOCX_ALIGN(boundary));

  return gas_local_memalign(here->gas, boundary, size);
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
  if (!size || !alignment) {
    *memptr = NULL;
    return 0;
  }

  if (!here || !here->btt) {
    *memptr = hpx_mallocx(size, MALLOCX_ALIGN(alignment));
    return (*memptr == 0) ? ENOMEM : 0;
  }

  return gas_local_posix_memalign(here->gas, memptr, alignment, size);
}
