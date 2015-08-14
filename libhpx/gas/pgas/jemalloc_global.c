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
#include "global.h"
#include "heap.h"

/// @file  libhpx/gas/pgas/global.c
/// @brief This file implements the address-space allocator interface for global
///        memory management.
///
/// In practice this is trivial. We just use these to bind the heap_chunk
/// allocation routines to the global_heap instance.
static void *
_global_chunk_alloc(void *addr, size_t n, size_t align, bool *z, unsigned arena)
{
  dbg_assert(global_heap);
  void *chunk = heap_chunk_alloc(global_heap, addr, n, align);
  if (z && *z && chunk) {
    memset(chunk, 0, n);
  }
  return chunk;
}

static bool
_global_chunk_free(void *addr, size_t n, unsigned arena) {
  dbg_assert(global_heap);
  heap_chunk_dalloc(global_heap, addr, n);
  return 0;
}

static bool
_global_chunk_purge(void *addr, size_t offset, size_t size, unsigned arena) {
  log_error("purging global memory is currently unsupported\n");
  return 1;
}

static chunk_allocator_t _global_allocator = {
  .challoc = _global_chunk_alloc,
  .chfree  = _global_chunk_free,
  .chpurge = _global_chunk_purge
};

void
global_allocator_init(void) {
  as_set_allocator(AS_GLOBAL, &_global_allocator);
}
