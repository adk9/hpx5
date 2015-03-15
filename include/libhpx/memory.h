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
#include <libhpx/debug.h>
#include <libhpx/system.h>

/// Forward declarations.
/// @{
struct config;
/// }@

/// This abstract class defines the external interface to an address space.
typedef struct address_space {
  void  (*delete)(void *space);
  void  (*join)(void *space);
  void  (*leave)(void *space);
  void  (*free)(void *addr);
  void *(*malloc)(size_t bytes);
  void *(*calloc)(size_t n, size_t bytes);
  void *(*memalign)(size_t boundary, size_t size);
} address_space_t;

/// These function types are used to parameterize the implementation of some of
/// the address spaces.
typedef int (*memory_register_t)(void *obj, void *base, size_t n, void *key);
typedef int (*memory_release_t)(void *obj, void *base, size_t n);

address_space_t *address_space_new_default(const struct config *cfg)
  HPX_INTERNAL;

address_space_t *address_space_new_jemalloc_registered(const struct config *cfg,
                                                       void *xport,
                                                       memory_register_t pin,
                                                       memory_release_t unpin,
                                                       void *mmap_obj,
                                                       system_mmap_t mmap,
                                                       system_munmap_t munmap)
  HPX_INTERNAL;

address_space_t *address_space_new_jemalloc_global(const struct config *cfg)
// ,
//                                                    void *xport,
//                                                    memory_register_t pin,
//                                                    memory_release_t unpin,
//                                                    void *mmap_obj,
//                                                    system_mmap_t mmap,
//                                                    system_munmap_t munmap)
  HPX_INTERNAL;

extern address_space_t *local;
extern address_space_t *registered;
extern address_space_t *global;

static inline void local_free(void *p) {
  dbg_assert(local && local->free);
  local->free(p);
}

static inline void *local_malloc(size_t bytes) {
  dbg_assert(local && local->malloc);
  return local->malloc(bytes);
}

static inline void *local_calloc(size_t n, size_t bytes) {
  dbg_assert(local && local->calloc);
  return local->calloc(n, bytes);
}

static inline void *local_memalign(size_t boundary, size_t size) {
  dbg_assert(local && local->memalign);
  return local->memalign(boundary, size);
}

static inline void registered_free(void *p) {
  dbg_assert(registered && registered->free);
  registered->free(p);
}

static inline void *registered_malloc(size_t bytes) {
  dbg_assert(registered && registered->malloc);
  return registered->malloc(bytes);
}

static inline void *registered_calloc(size_t n, size_t bytes) {
  dbg_assert(registered && registered->calloc);
  return registered->calloc(n, bytes);
}

static inline void *registered_memalign(size_t boundary, size_t size) {
  dbg_assert(registered && registered->memalign);
  return registered->memalign(boundary, size);
}

static inline void global_free(void *p) {
  dbg_assert(global && global->free);
  global->free(p);
}

static inline void *global_malloc(size_t bytes) {
  dbg_assert(global && global->malloc);
  return global->malloc(bytes);
}

static inline void *global_calloc(size_t n, size_t bytes) {
  dbg_assert(global && global->calloc);
  return global->calloc(n, bytes);
}

static inline void *global_memalign(size_t boundary, size_t size) {
  dbg_assert(global && global->memalign);
  return global->memalign(boundary, size);
}

#endif // LIBHPX_MEMORY_H
