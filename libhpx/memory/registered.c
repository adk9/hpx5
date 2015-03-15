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

#ifndef HAVE_JEMALLOC_REGISTERED
# error registered implementation should not be compiled without a network
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

static address_space_t _registered_address_space_vtable;

/// jemalloc requires static callbacks for chunk allocation. We need to know the
/// transport-specific mechanism for registering and releasing memory, so we use
/// these globals to store the callback. We'd prefer to be able to make the
/// member variables of the registered address space class in the future.
static            void *_xport = NULL;
static memory_register_t  _pin = NULL;
static memory_release_t _unpin = NULL;
static         void *_mmap_obj = NULL;
static system_mmap_t     _mmap = NULL;
static system_munmap_t _munmap = NULL;

/// Each thread needs to join and leave the address space. We remember the
/// thread's primordial arena as an indication that it already joined. 0 is a
/// valid arena, so we use UINT_MAX to indicate an unset handler.
static __thread unsigned _primordial_arena = UINT_MAX;

/// The registered memory chunk allocation callback.
static void *_registered_chunk_alloc(void *addr, size_t n, size_t align,
                                     bool *zero, unsigned arena) {
  return common_chunk_alloc(addr, n, align, zero, arena, _mmap_obj, _mmap,
                            _xport, _pin);
}

/// The registered memory chunk de-allocation callback.
static bool _registered_chunk_dalloc(void *chunk, size_t n, unsigned arena) {
  return common_chunk_dalloc(chunk, n, arena, _mmap_obj, _munmap, _xport,
                             _unpin);
}

/// A no-op delete function, since we're not allocating an object.
static void _registered_delete(void *space) {
  dbg_assert(space == &_registered_address_space_vtable);
}

/// Join the registered address space.
static void _registered_join(void *space) {
  common_join(space, &_registered_address_space_vtable, &_primordial_arena,
              libhpx_registered_mallctl,
              (void*)&_registered_chunk_alloc,
              (void*)&_registered_chunk_dalloc);
}

/// Leave the registered address space.
static void _registered_leave(void *space) {
  dbg_assert(space == &_registered_address_space_vtable);
}

/// The registered address space class.
static address_space_t _registered_address_space_vtable = {
  .delete = _registered_delete,
  .join = _registered_join,
  .leave = _registered_leave,
  .free = libhpx_registered_free,
  .malloc = libhpx_registered_malloc,
  .calloc = libhpx_registered_calloc,
  .memalign = libhpx_registered_memalign
};

address_space_t *
address_space_new_jemalloc_registered(const struct config *UNUSED,
                                      void *xport,
                                      memory_register_t pin,
                                      memory_release_t unpin,
                                      void *mmap_obj,
                                      system_mmap_t mmap,
                                      system_munmap_t munmap) {
  dbg_assert(!_xport || _xport == xport);
  dbg_assert(!_pin || _pin == pin);
  dbg_assert(!_unpin || _unpin == unpin);
  dbg_assert(!_mmap_obj || _mmap_obj == mmap_obj);
  dbg_assert(!_mmap || _mmap == mmap);
  dbg_assert(!_munmap || _munmap == munmap);

  _xport = xport;
  _pin = pin;
  _unpin = unpin;
  _mmap_obj = mmap_obj;
  _mmap = mmap;
  _munmap = munmap;

  return &_registered_address_space_vtable;
}
