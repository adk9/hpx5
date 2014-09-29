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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdbool.h>
#include <stdio.h>
#include <jemalloc/jemalloc.h>
#include "libhpx/debug.h"
#include "mallctl.h"

bool lhpx_mallctl_get_lg_dirty_mult(void) {
  ssize_t enabled = false;
  size_t sz = sizeof(enabled);
  int e = mallctl("opt.lg_dirty_mult", &enabled, &sz, NULL, 0);
  if (e)
    dbg_error("jemalloc: failed to check opt.lg_dirty_mult.\n");
  return enabled;
}

size_t lhpx_mallctl_get_chunk_size(void) {
  size_t log2_bytes_per_chunk = 0;
  size_t sz = sizeof(log2_bytes_per_chunk);
  int e = mallctl("opt.lg_chunk", &log2_bytes_per_chunk, &sz, NULL, 0);
  if (e)
    dbg_error("jemalloc: failed to read the chunk size\n");

  return 1 << log2_bytes_per_chunk;
}

unsigned lhpx_mallctl_create_arena(chunk_alloc_t alloc, chunk_dalloc_t dalloc) {
  unsigned arena = 0;
  size_t sz = sizeof(arena);
  int e = mallctl("arenas.extend", &arena, &sz, NULL, 0);
  if (e)
    dbg_error("jemalloc: failed to allocate a new arena %d.\n", e);

  char path[128];
  snprintf(path, 128, "arena.%u.chunk.alloc", arena);
  e = mallctl(path, NULL, NULL, (void*)&alloc, sizeof(alloc));
  if (e)
    dbg_error("jemalloc: failed to set chunk allocator on arena %u\n", arena);

  snprintf(path, 128, "arena.%u.chunk.dalloc", arena);;
  e = mallctl(path, NULL, NULL, (void*)&dalloc, sizeof(dalloc));
  if (e)
    dbg_error("jemalloc: failed to set chunk dallocator on arena %u\n", arena);

  return arena;
}

unsigned lhpx_mallctl_thread_get_arena(void) {
  unsigned arena = 0;
  size_t sz = sizeof(arena);
  int e = mallctl("thread.arena", &arena, &sz, NULL, 0);
  if (e)
    dbg_error("jemalloc: failed to read the default arena\n");
  return arena;
}

unsigned lhpx_mallctl_thread_set_arena(unsigned arena) {
  unsigned old = 0;
  size_t sz1 = sizeof(arena), sz2 = sizeof(arena);
  int e = mallctl("thread.arena", &old , &sz1, &arena, sz2);
  if (e)
    dbg_error("jemalloc: failed to update the default arena\n");
  return old;
}

void lhpx_mallctl_thread_enable_cache(void) {
  bool enable = true;
  size_t sz = sizeof(enable);
  int e = mallctl("thread.tcache.enabled", NULL, NULL, &enable, sz);
  if (e)
    dbg_error("jemalloc: failed to enable the thread cache.\n");
}

void lhpx_mallctl_thread_flush_cache(void) {
  int e = mallctl("thread.tcache.flush", NULL, NULL, NULL, 0);
  if (e)
    dbg_error("jemalloc: failed to flush thread cache.\n");
}
