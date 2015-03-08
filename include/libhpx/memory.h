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
#ifndef LIBHPX_MEMORY_H
#define LIBHPX_MEMORY_H

/// @file  libhpx/memory.h
///
/// @brief This file contains declarations for allocation from the various
///        memory spaces that HPX uses for its local allocations.
///
///        Registered memory is memory that can be used in RDMA operations, but
///        that doesn't need a global address mapping. While any memory can be
///        registered using the network_register() operation, the registered
///        memory pool should be used for object classes that stay registered,
///        like parcels, stacks, etc.
///
///        Global memory is memory that has a corresponding GAS address. It is
///        also registered with the network.
#include <stddef.h>
#include <hpx/attributes.h>

void global_free(void *p)
  HPX_INTERNAL;

void *global_malloc(size_t bytes)
  HPX_INTERNAL HPX_MALLOC;

void *global_memalign(size_t boundary, size_t size)
  HPX_INTERNAL HPX_MALLOC;

void registered_free(void *p)
  HPX_INTERNAL;

void *registered_malloc(size_t bytes)
  HPX_INTERNAL HPX_MALLOC;

void *registered_memalign(size_t boundary, size_t size)
  HPX_INTERNAL HPX_MALLOC;

#endif // LIBHPX_MEMORY_H
