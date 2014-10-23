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
#include <sys/types.h>
#include <jemalloc/jemalloc.h>
#include "libhpx/debug.h"
#include "mallctl.h"

int mallctl_get_lg_dirty_mult(void) {
  ssize_t opt = -1;
  size_t sz = sizeof(opt);
  int e = libhpx_global_mallctl("opt.lg_dirty_mult", &opt, &sz, NULL, 0);
  if (e)
    dbg_error("jemalloc: failed to check opt.lg_dirty_mult.\n");
  return (int)opt;
}

/// This is a hack to force jemalloc to avoid purging pages. The
/// opt.lg_dirty_mult is a read-only config value, but we can force it to be
/// what we want because we're using an embedded jemalloc.
extern ssize_t je_opt_lg_dirty_mult;

bool mallctl_disable_dirty_page_purge(void) {
  je_opt_lg_dirty_mult = -1;
  return (mallctl_get_lg_dirty_mult() == -1);
}


size_t mallctl_get_chunk_size(void) {
  size_t log2_bytes_per_chunk = 0;
  size_t sz = sizeof(log2_bytes_per_chunk);
  int e = libhpx_global_mallctl("opt.lg_chunk", &log2_bytes_per_chunk, &sz,
                                NULL, 0);
  if (e)
    dbg_error("jemalloc: failed to read the chunk size\n");

  return 1 << log2_bytes_per_chunk;
}

unsigned mallctl_create_arena(chunk_alloc_t alloc, chunk_dalloc_t dalloc) {
  unsigned arena = 0;
  size_t sz = sizeof(arena);
  int e = libhpx_global_mallctl("arenas.extend", &arena, &sz, NULL, 0);
  if (e)
    dbg_error("jemalloc: failed to allocate a new arena %d.\n", e);

  if (alloc) {
    char path[128];
    snprintf(path, 128, "arena.%u.chunk.alloc", arena);
    e = libhpx_global_mallctl(path, NULL, NULL, (void*)&alloc, sizeof(alloc));
    if (e)
      dbg_error("jemalloc: failed to set chunk allocator on arena %u\n", arena);
  }

  if (dalloc) {
    char path[128];
    snprintf(path, 128, "arena.%u.chunk.dalloc", arena);;
    e = libhpx_global_mallctl(path, NULL, NULL, (void*)&dalloc, sizeof(dalloc));
    if (e)
      dbg_error("jemalloc: failed to set chunk dallocator on arena %u\n", arena);
  }

  return arena;
}

unsigned mallctl_thread_get_arena(void) {
  unsigned arena = 0;
  size_t sz = sizeof(arena);
  int e = libhpx_global_mallctl("thread.arena", &arena, &sz, NULL, 0);
  if (e)
    dbg_error("jemalloc: failed to read the default arena\n");
  return arena;
}

unsigned mallctl_thread_set_arena(unsigned arena) {
  unsigned old = 42;
  size_t sz1 = sizeof(arena), sz2 = sizeof(arena);
  int e = libhpx_global_mallctl("thread.arena", &old , &sz1, NULL, 0);
  assert(!e);
  e = libhpx_global_mallctl("thread.arena", NULL, NULL, &arena, sz2);
  if (e)
    dbg_error("jemalloc: failed to update the default arena\n");
  return old;
}

void mallctl_thread_enable_cache(void) {
  bool enable = true;
  size_t sz = sizeof(enable);
  int e = libhpx_global_mallctl("thread.tcache.enabled", NULL, NULL, &enable,
                                sz);
  if (e)
    dbg_error("jemalloc: failed to enable the thread cache.\n");
}

void mallctl_thread_flush_cache(void) {
  int e = libhpx_global_mallctl("thread.tcache.flush", NULL, NULL, NULL, 0);
  if (e)
    dbg_error("jemalloc: failed to flush thread cache.\n");
}
