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

#include <stdio.h>
#include <libhpx/debug.h>
#include <libhpx/memory.h>

const char *je_malloc_conf = "lg_dirty_mult:-1,lg_chunk:22";

/// Backing declaration for the flags.
__thread int as_flags[AS_COUNT] = {0};

/// Each address space can have a custom allocator configured. This array stores
/// the allocator pointer for each address space.
/// @{
static chunk_allocator_t *_allocators[AS_COUNT] = {NULL};
/// @}

void
as_set_allocator(int id, chunk_allocator_t *allocator) {
  dbg_assert(0 <= id && id < AS_COUNT);
  dbg_assert(allocator);
  dbg_assert(!_allocators[id]);
  _allocators[id] = allocator;
  as_join(id);
}

void
as_join(int id) {
  if (as_flags[id] != 0) {
    log_gas("address space %d already joined\n", id);
    return;
  }

  chunk_allocator_t *allocator = _allocators[id];

  // If there isn't any custom allocator set for this space, then the basic
  // local allocator is fine, which means that we don't need any special
  // flags for this address space.
  if (!allocator) {
    log_gas("no custom allocator for %d, using local\n", id);
    return;
  }

  // Create an arena that uses the right allocators.
  unsigned arena;
  size_t sz = sizeof(arena);
  je_mallctl("arenas.extend", &arena, &sz, NULL, 0);


  char path[128];
  snprintf(path, 128, "arena.%u.chunk.alloc", arena);
  je_mallctl(path, NULL, NULL, (void*)&allocator->challoc, sizeof(void*));

  snprintf(path, 128, "arena.%u.chunk.dalloc", arena);
  je_mallctl(path, NULL, NULL, (void*)&allocator->chfree, sizeof(void*));

  snprintf(path, 128, "arena.%u.chunk.purge", arena);
  je_mallctl(path, NULL, NULL, (void*)&allocator->chpurge, sizeof(void*));

  // Create a cache.
  unsigned cache;
  sz = sizeof(cache);
  je_mallctl("tcache.create", &cache, &sz, NULL, 0);

  // And set the flags.
  as_flags[id] = MALLOCX_ARENA(arena) | MALLOCX_TCACHE(cache);
}

void
as_leave(void) {
}

size_t
as_bytes_per_chunk(void) {
  size_t log2_bytes_per_chunk = 0;
  size_t sz = sizeof(log2_bytes_per_chunk);
  je_mallctl("opt.lg_chunk", &log2_bytes_per_chunk, &sz, NULL, 0);
  return (1lu << log2_bytes_per_chunk);
}
