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
#ifndef LIBHPX_GAS_AGAS_H
#define LIBHPX_GAS_AGAS_H

#include <hpx/attributes.h>

struct boot;
struct config;
struct gas;

struct gas *gas_agas_new(const struct config *config, struct boot *boot)
  HPX_INTERNAL;

/// Asynchronous entry point for alloc.
/// type hpx_addr_t (size_t bytes, size_t align)
HPX_INTERNAL extern HPX_ACTION_DECL(agas_alloc_cyclic);

/// Asynchronous entry point for calloc.
/// type hpx_addr_t (size_t bytes, size_t align)
HPX_INTERNAL extern HPX_ACTION_DECL(agas_calloc_cyclic);

/// Synchronous entry point for alloc.
///
/// @param            n The total number of bytes to allocate.
/// @param        bsize The size of each block, in bytes.
///
/// @returns            A global address representing the base of the
///                     allocation, or HPX_NULL if there is an error.
hpx_addr_t agas_alloc_cyclic_sync(size_t n, uint32_t bsize)
  HPX_INTERNAL;

/// Synchronous entry point for calloc.
///
/// @param            n The total number of bytes to allocate.
/// @param        bsize The size of each block, in bytes.
///
/// @returns            A global address representing the base of the
///                     allocation, or HPX_NULL if there is an error.
hpx_addr_t agas_calloc_cyclic_sync(size_t n, uint32_t bsize)
  HPX_INTERNAL;


#endif
