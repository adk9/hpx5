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

#include <libhpx/debug.h>
#include "common.h"

void *common_chunk_alloc(void *addr, size_t size, size_t align, bool *zero,
                         unsigned arena, void *mmap_obj, system_mmap_t mmap,
                         void *xport, memory_register_t pin) {
  dbg_assert(!addr || !((uintptr_t)addr & (align -1)));
  void *chunk = mmap(mmap_obj, addr, size, align);

  // If we found nothing, anywhere in memory, then we have a problem.
  if (!chunk) {
    dbg_error("failed to mmap %zu bytes anywhere in memory\n", size);
  }

  // Our mmap interface guarantees alignment, so just go ahead and register it
  // and return.
  int e = pin(xport, chunk, size, NULL);
  dbg_check(e);
  if (zero && *zero) {
    memset(chunk, 0, size);
  }
  else {
    *zero = false;
  }
  return chunk;
}

bool common_chunk_dalloc(void *chunk, size_t size, unsigned arena,
                         void *mmap_obj, system_munmap_t munmap,
                         void *xport, memory_release_t unpin) {
  int e = unpin(xport, chunk, size);
  dbg_check(e, "\n");
  munmap(mmap_obj, chunk, size);
  return true;
}

void call(mallctl_t mallctl, const char *str, void *old, size_t *oldn,
          void *new, size_t newn) {
  int e = mallctl(str, old, oldn, new, newn);
  if (e) {
    dbg_error("failed registered mallctl(%s).\n", str);
  }
}

void common_join(void *space, const void *class, unsigned *primordial_arena,
                 mallctl_t mallctl, void *alloc, void *dalloc)
{
  // We know that the registered space is a singleton object, so we make sure
  // it's been allocated and that it's the right "class".
  dbg_assert(space == class);
  dbg_assert(primordial_arena);

  // If we've (the current pthread) already joined this address space then we
  // will have stored our primordial arena.
  if (*primordial_arena != UINT_MAX) {
    return;
  }

  // Verify that the user hasn't overridden the lg_dirty_mult flags.
  ssize_t opt = 0;
  size_t sz = sizeof(opt);
  call(mallctl, "opt.lg_dirty_mult", &opt, &sz, NULL, 0);
  if (opt != -1) {
    dbg_error("jemalloc instance expects MALLOC_CONF=lg_dirty_mult:-1");
  }

  // Create an arena that uses the right allocators.
  unsigned arena = 0;
  sz = sizeof(arena);
  call(mallctl, "arenas.extend", &arena, &sz, NULL, 0);

  if (alloc) {
    char path[128];
    snprintf(path, 128, "arena.%u.chunk.alloc", arena);
    call(mallctl, path, NULL, NULL, (void*)&alloc, sizeof(alloc));
  }

  if (dalloc) {
    char path[128];
    snprintf(path, 128, "arena.%u.chunk.dalloc", arena);;
    call(mallctl, path, NULL, NULL, (void*)&dalloc, sizeof(dalloc));
  }

  // Enable and flush the cache.
  bool enabled = true;
  call(mallctl, "thread.tcache.enabled", NULL, NULL, &enabled, sizeof(enabled));
  call(mallctl, "thread.tcache.flush", NULL, NULL, NULL, 0);

  // And replace the current arena.
  sz = sizeof(*primordial_arena);
  call(mallctl, "thread.arena", primordial_arena, &sz, &arena, sizeof(arena));
}
