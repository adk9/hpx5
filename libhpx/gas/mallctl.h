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
#ifndef LIBHPX_GAS_MALLCTL_H
#define LIBHPX_GAS_MALLCTL_H

#include <stddef.h>
#include <hpx/attributes.h>
#include <jemalloc/jemalloc.h>

bool lhpx_mallctl_get_lg_dirty_mult(void)
  HPX_INTERNAL;

size_t lhpx_mallctl_get_chunk_size(void)
  HPX_INTERNAL;

unsigned lhpx_mallctl_create_arena(chunk_alloc_t alloc, chunk_dalloc_t dalloc)
  HPX_INTERNAL;

unsigned lhpx_mallctl_thread_get_arena(void)
  HPX_INTERNAL;

unsigned lhpx_mallctl_thread_set_arena(unsigned)
  HPX_INTERNAL;

void lhpx_mallctl_thread_enable_cache(void)
  HPX_INTERNAL;

void lhpx_mallctl_thread_flush_cache(void)
  HPX_INTERNAL;

#endif // LIBHPX_GAS_MALLCTL_H
