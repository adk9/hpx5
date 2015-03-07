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
#ifdef HAVE_JEMALLOC_GLOBAL
# include <libhpx/jemalloc_global.h>
static inline void global_free(void *p) {
  libhpx_global_free(p);
}

static inline void *global_malloc(size_t bytes) {
  return libhpx_global_malloc(bytes);
}
#else
# include <stdlib.h>
static inline void global_free(void *p) {
  free(p);
}

static inline void *global_malloc(size_t bytes) {
  return malloc(bytes);
}
#endif

#ifdef HAVE_JEMALLOC_REGISTERED
# include <libhpx/jemalloc_registered.h>
static inline void registered_free(void *addr) {
  libhpx_registered_free(addr);
}

static inline void *registered_memalign(size_t boundary, size_t size) {
  return libhpx_registered_memalign(boundary, size);
}

#else
# include <stdlib.h>
static inline void registered_free(void *addr) {
  free(addr);
}

static inline void *registered_memalign(size_t boundary, size_t size) {
  void *p;
  posix_memalign(&p, boundary, size);
  return p;
}
#endif

#endif // LIBHPX_MEMORY_H
