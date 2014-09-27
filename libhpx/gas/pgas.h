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
#ifndef LIBHPX_GAS_PGAS_H
#define LIBHPX_GAS_PGAS_H

/// @file libhpx/gas/pgas.h
/// @brief PGAS specific interface to the global address space.
///
/// In HPX, allocations are done in terms of "blocks," which are user-sized,
/// byte-addressible buffers.
///
/// In the PGAS model, global address values uniquely identify blocks that
/// remain fixed at the locality at which they were originally allocated.
///
/// We distinguish two forms of allocation, cyclic and acyclic. Cyclic
/// allocations are arrays of blocks that have contiguous global addresses but
/// are distributed round-robin across localities. Cyclic allocation regions
/// always start with the root node. Acyclic allocations are arrays of blocks
/// that have contiguous global addresses but are allocated on only one node.
///

#include <stddef.h>
#include <hpx/attributes.h>

int lhpx_pgas_init(size_t heap_size)
  HPX_INTERNAL;

int lhpx_pgas_init_worker()
  HPX_INTERNAL;

void lphx_pgas_fini_worker()
  HPX_INTERNAL;

void lhpx_pgas_fini(void)
  HPX_INTERNAL;

void *lhpx_pgas_malloc(size_t bytes)
  HPX_INTERNAL;

void lhpx_pgas_free(void *ptr)
  HPX_INTERNAL;

void *lhpx_pgas_calloc(size_t nmemb, size_t size)
  HPX_INTERNAL;

void *lhpx_pgas_realloc(void *ptr, size_t size)
  HPX_INTERNAL;

void *lhpx_pgas_valloc(size_t size)
  HPX_INTERNAL;

void *lhpx_pgas_memalign(size_t boundary, size_t size)
  HPX_INTERNAL;

int lhpx_pgas_posix_memalign(void **memptr, size_t alignment, size_t size)
  HPX_INTERNAL;

#endif
