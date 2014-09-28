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
#include <hpx/builtins.h>
#include <hpx/malloc.h>
#include "libhpx/btt.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "smp.h"
#include "pgas.h"

void *malloc(size_t bytes) {
  if (!bytes)
    return NULL;

  if (!here || !here->btt)
    return mallocx(bytes, 0);

  switch (here->btt->type) {
   default:
    dbg_error("malloc: unexpected GAS type %u\n", here->btt->type);
    hpx_abort();

   case (HPX_GAS_NOGLOBAL):
    return lhpx_smp_malloc(bytes);

   case (HPX_GAS_PGAS):
    return lhpx_pgas_malloc(bytes);

   case (HPX_GAS_AGAS):
   case (HPX_GAS_PGAS_SWITCH):
   case (HPX_GAS_AGAS_SWITCH):
    return mallocx(bytes, 0);
  }
  unreachable();
}

void free(void *ptr) {
  if (!ptr)
    return;

  if (!here || !here->btt) {
    dallocx(ptr, 0);
    return;
  }

  switch (here->btt->type) {
   default:
    dbg_error("malloc: unexpected GAS type %u\n", here->btt->type);
    hpx_abort();

   case (HPX_GAS_NOGLOBAL):
    lhpx_smp_free(ptr);
    return;

   case (HPX_GAS_PGAS):
    lhpx_pgas_free(ptr);
    return;

   case (HPX_GAS_AGAS):
   case (HPX_GAS_PGAS_SWITCH):
   case (HPX_GAS_AGAS_SWITCH):
    dallocx(ptr, 0);
    return;
  }
}


void *calloc(size_t nmemb, size_t size) {
  if (!nmemb || !size)
    return NULL;

  if (!here || !here->btt)
    return mallocx(nmemb * size, MALLOCX_ZERO);

  switch (here->btt->type) {
   default:
    dbg_error("malloc: unexpected GAS type %u\n", here->btt->type);
    hpx_abort();

   case (HPX_GAS_NOGLOBAL):
    return lhpx_smp_calloc(nmemb, size);

   case (HPX_GAS_PGAS):
    return lhpx_pgas_calloc(nmemb, size);

   case (HPX_GAS_AGAS):
   case (HPX_GAS_PGAS_SWITCH):
   case (HPX_GAS_AGAS_SWITCH):
    return mallocx(nmemb * size, MALLOCX_ZERO);
  }
  unreachable();
}

void *realloc(void *ptr, size_t size) {
  if (!ptr)
    return malloc(size);

  if (!here || !here->btt)
    return rallocx(ptr, size, 0);

  switch (here->btt->type) {
   default:
    dbg_error("malloc: unexpected GAS type %u\n", here->btt->type);
    hpx_abort();

   case (HPX_GAS_NOGLOBAL):
    return lhpx_smp_realloc(ptr, size);

   case (HPX_GAS_PGAS):
    return lhpx_pgas_realloc(ptr, size);

   case (HPX_GAS_AGAS):
   case (HPX_GAS_PGAS_SWITCH):
   case (HPX_GAS_AGAS_SWITCH):
    return rallocx(ptr, size, 0);
  }
  unreachable();
}

void *valloc(size_t size) {
  if (!here || !here->btt)
    return mallocx(size, MALLOCX_ALIGN(HPX_PAGE_SIZE));

  switch (here->btt->type) {
   default:
    dbg_error("malloc: unexpected GAS type %u\n", here->btt->type);
    hpx_abort();

   case (HPX_GAS_NOGLOBAL):
    return lhpx_smp_valloc(size);

   case (HPX_GAS_PGAS):
    return lhpx_pgas_valloc(size);

   case (HPX_GAS_AGAS):
   case (HPX_GAS_PGAS_SWITCH):
   case (HPX_GAS_AGAS_SWITCH):
    return mallocx(size, MALLOCX_ALIGN(HPX_PAGE_SIZE));
  }
  unreachable();
}

void *memalign(size_t boundary, size_t size) {
  if (!here || !here->btt)
    return mallocx(size, MALLOCX_ALIGN(boundary));

  switch (here->btt->type) {
   default:
    dbg_error("malloc: unexpected GAS type %u\n", here->btt->type);
    hpx_abort();

   case (HPX_GAS_NOGLOBAL):
    return lhpx_smp_memalign(boundary, size);

   case (HPX_GAS_PGAS):
    return lhpx_pgas_memalign(boundary, size);

   case (HPX_GAS_AGAS):
   case (HPX_GAS_PGAS_SWITCH):
   case (HPX_GAS_AGAS_SWITCH):
    return mallocx(size, MALLOCX_ALIGN(boundary));
  }
  unreachable();
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
  if (!here || !here->btt) {
    *memptr = mallocx(size, MALLOCX_ALIGN(alignment));
    return (*memptr == 0) ? ENOMEM : 0;
  }

  switch (here->btt->type) {
   default:
    dbg_error("malloc: unexpected GAS type %u\n", here->btt->type);
    hpx_abort();
   case (HPX_GAS_NOGLOBAL):
    return lhpx_smp_posix_memalign(memptr, alignment, size);

   case (HPX_GAS_PGAS):
    return lhpx_pgas_posix_memalign(memptr, alignment, size);

   case (HPX_GAS_AGAS):
   case (HPX_GAS_PGAS_SWITCH):
   case (HPX_GAS_AGAS_SWITCH):
    *memptr = mallocx(size, MALLOCX_ALIGN(alignment));
    return (*memptr == 0) ? ENOMEM : 0;
  }
  unreachable();
}
