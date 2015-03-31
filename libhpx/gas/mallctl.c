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

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <jemalloc/jemalloc_global.h>

#define call_mallctl(str, ...)                          \
  do {                                                  \
    int e = libhpx_global_mallctl(str, __VA_ARGS__);    \
    if (e) {                                            \
      dbg_error("failed mallctl(%s).\n", str);          \
    }                                                   \
  } while (0)

int mallctl_get_lg_dirty_mult(void) {
  ssize_t opt = -1;
  size_t sz = sizeof(opt);
  call_mallctl("opt.lg_dirty_mult", &opt, &sz, NULL, 0);
  return (int)opt;
}

int mallctl_disable_dirty_page_purge(void) {
  return (mallctl_get_lg_dirty_mult() == -1) ? LIBHPX_OK : LIBHPX_ERROR;
}

size_t mallctl_get_chunk_size(void) {
  size_t log2_bytes_per_chunk = 0;
  size_t sz = sizeof(log2_bytes_per_chunk);
  call_mallctl("opt.lg_chunk", &log2_bytes_per_chunk, &sz, NULL, 0);
  return (size_t)1 << log2_bytes_per_chunk;
}

unsigned mallctl_create_arena(chunk_alloc_t alloc, chunk_dalloc_t dalloc) {
  unsigned arena = 0;
  size_t sz = sizeof(arena);
  call_mallctl("arenas.extend", &arena, &sz, NULL, 0);

  if (alloc) {
    char path[128];
    snprintf(path, 128, "arena.%u.chunk.alloc", arena);
    call_mallctl(path, NULL, NULL, (void*)&alloc, sizeof(alloc));
  }

  if (dalloc) {
    char path[128];
    snprintf(path, 128, "arena.%u.chunk.dalloc", arena);;
    call_mallctl(path, NULL, NULL, (void*)&dalloc, sizeof(dalloc));
  }

  return arena;
}

unsigned mallctl_thread_get_arena(void) {
  unsigned arena = 0;
  size_t sz = sizeof(arena);
  call_mallctl("thread.arena", &arena, &sz, NULL, 0);
  return arena;
}

unsigned mallctl_thread_set_arena(unsigned arena) {
  unsigned old = 42;
  size_t sz1 = sizeof(arena), sz2 = sizeof(arena);
  call_mallctl("thread.arena", &old , &sz1, NULL, 0);
  call_mallctl("thread.arena", NULL, NULL, &arena, sz2);
  return old;
}

void mallctl_thread_enable_cache(void) {
  bool enable = true;
  size_t sz = sizeof(enable);
  call_mallctl("thread.tcache.enabled", NULL, NULL, &enable, sz);
}

void mallctl_thread_flush_cache(void) {
  call_mallctl("thread.tcache.flush", NULL, NULL, NULL, 0);
}
