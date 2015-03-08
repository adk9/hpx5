// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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

#include <hpx/attributes.h>
#include <jemalloc/jemalloc_global.h>

int mallctl_get_lg_dirty_mult(void)
  HPX_INTERNAL;

int mallctl_disable_dirty_page_purge(void)
  HPX_INTERNAL;

size_t mallctl_get_chunk_size(void)
  HPX_INTERNAL;

unsigned mallctl_create_arena(chunk_alloc_t alloc, chunk_dalloc_t dalloc)
  HPX_INTERNAL;

unsigned mallctl_thread_get_arena(void)
  HPX_INTERNAL;

unsigned mallctl_thread_set_arena(unsigned arena)
  HPX_INTERNAL;

void mallctl_thread_enable_cache(void)
  HPX_INTERNAL;

void mallctl_thread_flush_cache(void)
  HPX_INTERNAL;

#endif // LIBHPX_GAS_MALLCTL_H
