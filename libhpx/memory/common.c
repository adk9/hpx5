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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libhpx/debug.h>
#include "common.h"

void *common_chunk_alloc(void *obj, void *addr, size_t size, size_t align,
                         bool *zero, unsigned arena) {
  dbg_assert(!addr || !((uintptr_t)addr & (align -1)));
  common_allocator_t *common = obj;
  void *chunk = common->mmap(common->mmap_obj, addr, size, align);

  // If we found nothing, anywhere in memory, then we have a problem.
  if (!chunk) {
    dbg_error("failed to mmap %zu bytes anywhere in memory\n", size);
  }

  // Our mmap interface guarantees alignment, so just go ahead and register it
  // and return.
  int e = common->pin(common->xport, chunk, size, NULL);
  dbg_check(e, "\n");
  if (zero && *zero) {
    memset(chunk, 0, size);
  }
  else if (zero) {
    *zero = false;
  }
  return chunk;
}

bool common_chunk_dalloc(void *obj, void *chunk, size_t size, unsigned arena) {
  common_allocator_t *common = obj;
  int e = common->unpin(common->xport, chunk, size);
  dbg_check(e, "\n");
  common->munmap(common->mmap_obj, chunk, size);
  return true;
}

void mallctl(void *obj, const char *str, void *old, size_t *oldn, void *new,
             size_t newn) {
  common_allocator_t *common = obj;
  int e = common->mallctl(str, old, oldn, new, newn);
  if (e) {
    dbg_error("failed registered mallctl(%s).\n", str);
  }
}

void common_join(void *obj, unsigned *primordial_arena, void *alloc,
                 void *dalloc) {
  dbg_assert(primordial_arena);

  // If we've (the current pthread) already joined this address space then we
  // will have stored our primordial arena.
  if (*primordial_arena != UINT_MAX) {
    return;
  }

  // Verify that the user hasn't overridden the lg_dirty_mult flags.
  ssize_t opt = 0;
  size_t sz = sizeof(opt);
  mallctl(obj, "opt.lg_dirty_mult", &opt, &sz, NULL, 0);
  if (opt != -1) {
    dbg_error("jemalloc instance expects MALLOC_CONF=lg_dirty_mult:-1");
  }

  // Create an arena that uses the right allocators.
  unsigned arena = 0;
  sz = sizeof(arena);
  mallctl(obj, "arenas.extend", &arena, &sz, NULL, 0);

  if (alloc) {
    char path[128];
    snprintf(path, 128, "arena.%u.chunk.alloc", arena);
    mallctl(obj, path, NULL, NULL, (void*)&alloc, sizeof(alloc));
  }

  if (dalloc) {
    char path[128];
    snprintf(path, 128, "arena.%u.chunk.dalloc", arena);;
    mallctl(obj, path, NULL, NULL, (void*)&dalloc, sizeof(dalloc));
  }

  // And replace the current arena.
  sz = sizeof(*primordial_arena);
  mallctl(obj, "thread.arena", primordial_arena, &sz, &arena, sizeof(arena));

  // Flush the cache.
  bool enabled = true;
  mallctl(obj, "thread.tcache.enabled", NULL, NULL, &enabled, sizeof(enabled));
  mallctl(obj, "thread.tcache.flush", NULL, NULL, NULL, 0);
}

void common_leave(void *common) {
}

void common_delete(void *common) {
  if (common) {
    free(common);
  }
}
