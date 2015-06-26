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

#include <string.h>
#include <libhpx/debug.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/system.h>
#include "registered.h"
#include "xport.h"

/// The transport we'll use for pinning. It's not ideal to stick it here, but we
/// need to pin before the network has been exposed through the
/// here->network. We can simply capture the transport since we know we'll need
/// it anyway.
static pwc_xport_t *_xport = NULL;

/// @file  libhpx/gas/pgas/registered.c
/// @brief This file implements the address-space allocator interface for
///        registered memory management.
///
/// In practice this is trivial. We just use these to bind the heap_chunk
/// allocation routines to the registered_heap instance.
static void *
_registered_chunk_alloc(void *addr, size_t n, size_t align, bool *zero,
                        unsigned arena)
{
  void *chunk = system_mmap_huge_pages(NULL, addr, n, align);
  if (!chunk) {
    dbg_error("failed to mmap %zu bytes anywhere in memory\n", n);
  }
  _xport->pin(chunk, n, NULL);

  if (zero && *zero) {
    memset(chunk, 0, n);
  }

  return chunk;
}

static bool
_registered_chunk_free(void *chunk, size_t n, unsigned arena) {
  _xport->unpin(chunk, n);
  system_munmap_huge_pages(NULL, chunk, n);
  return 0;
}

static bool
_registered_chunk_purge(void *addr, size_t offset, size_t size, unsigned arena) {
  log_mem("cannot purge registered memory\n");
  return 1;
}

static chunk_allocator_t _registered_allocator = {
  .challoc = _registered_chunk_alloc,
  .chfree  = _registered_chunk_free,
  .chpurge = _registered_chunk_purge
};

void
registered_allocator_init(pwc_xport_t *xport) {
  _xport = xport;
  as_set_allocator(AS_REGISTERED, &_registered_allocator);
}
