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
#ifndef LIBHPX_GAS_SMP_H
#define LIBHPX_GAS_SMP_H

/// @file libhpx/gas/smp.h
/// @brief SMP specific interface to the global address space.
///

#include <stddef.h>
#include <hpx/attributes.h>

int smp_init(size_t heap_size)
  HPX_INTERNAL;

int smp_init_worker()
  HPX_INTERNAL;

void lphx_smp_fini_worker()
  HPX_INTERNAL;

void smp_fini(void)
  HPX_INTERNAL;

void *smp_malloc(size_t bytes)
  HPX_INTERNAL;

void smp_free(void *ptr)
  HPX_INTERNAL;

void *smp_calloc(size_t nmemb, size_t size)
  HPX_INTERNAL;

void *smp_realloc(void *ptr, size_t size)
  HPX_INTERNAL;

void *smp_valloc(size_t size)
  HPX_INTERNAL;

void *smp_memalign(size_t boundary, size_t size)
  HPX_INTERNAL;

int smp_posix_memalign(void **memptr, size_t alignment, size_t size)
  HPX_INTERNAL;

#endif
