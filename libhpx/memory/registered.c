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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/// @file  libhpx/memory/registered.c
/// @brief An address space implementation that uses the jemalloc_registered
///        instance.
///
/// Registered memory is memory that we can use as the "local memory" side of a
/// gas memput/memget or network put/get/pwc. In some of our transports, this
/// requires that memory be actually registered with the network card. This
/// "jemalloc_registered" address-space class uses the jemalloc_registered
/// prefixed interface to jemalloc to manage registrations at the jemalloc
/// "chunk" level.
///
/// As each chunk is allocated, we need to use the chunk allocator to register
/// the appropriate memory. This is complicated by the fact that we would like
/// to use huge pages for registered memory, since the registration process
/// will mlock() all of the pages anyway, there's no real benefit to using
/// smaller pages in this context.
#include <jemalloc/jemalloc_registered.h>
#include <libhpx/memory.h>
#include "common.h"

/// Transports that actually register memory can't handle having page mappings
/// dropped. Currently, jemalloc doesn't expose this on a per-arena basis, nor
/// is it writable, so we have to force it "off" for the registered arena,
/// just-in-case.
///
/// NB: This is annoying for the case where the transport doesn't care about
///     registration, and is also annoying for the case where we aren't using
///     huge-tlb pages and might actually be interested in dropping
///     registrations when jemalloc says that we can.
const char *libhpx_registered_malloc_conf = "lg_dirty_mult:-1";

/// Each thread needs to join and leave the address space. We remember the
/// thread's primordial arena as an indication that it already joined. 0 is a
/// valid arena, so we use UINT_MAX to indicate an unset handler.
static __thread unsigned _primordial_arena = UINT_MAX;

/// The registered memory chunk allocation callback.
static void *_registered_chunk_alloc(void *addr, size_t n, size_t align,
                                     bool *zero, unsigned arena) {
  return common_chunk_alloc(registered, addr, n, align, zero, arena);
}

/// The registered memory chunk de-allocation callback.
static bool _registered_chunk_dalloc(void *chunk, size_t n, unsigned arena) {
  return common_chunk_dalloc(registered, chunk, n, arena);
}

/// Join the registered address space.
static void _registered_join(void *common) {
  void *alloc = (void*)&_registered_chunk_alloc;
  void *dalloc = (void*)&_registered_chunk_dalloc;
  common_join(common, &_primordial_arena, alloc, dalloc);
}

address_space_t *
address_space_new_jemalloc_registered(const struct config *UNUSED,
                                      void *xport,
                                      memory_register_t pin,
                                      memory_release_t unpin,
                                      void *mmap_obj,
                                      system_mmap_t mmap,
                                      system_munmap_t munmap) {
  // we cheat because we know that the registered manager is a singleton
  if (registered) {
    return registered;
  }

  common_allocator_t *allocator = malloc(sizeof(*allocator));
  dbg_assert(allocator);
  allocator->vtable.delete = common_delete;
  allocator->vtable.join = _registered_join;
  allocator->vtable.leave = common_leave;
  allocator->vtable.free = libhpx_registered_free;
  allocator->vtable.malloc = libhpx_registered_malloc;
  allocator->vtable.calloc = libhpx_registered_calloc;
  allocator->vtable.memalign = libhpx_registered_memalign;

  allocator->xport = xport;
  allocator->pin = pin;
  allocator->unpin = unpin;
  allocator->mmap_obj = mmap_obj;
  allocator->mmap = mmap;
  allocator->munmap = munmap;
  allocator->mallctl = libhpx_registered_mallctl;

  return &allocator->vtable;
}
