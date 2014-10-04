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

#include <limits.h>
#include <stdbool.h>
#include <jemalloc/jemalloc.h>

#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "../mallctl.h"
#include "../malloc.h"
#include "heap.h"
#include "pgas.h"

static __thread unsigned _global_arena = UINT_MAX;
static __thread unsigned _local_arena = UINT_MAX;
static __thread unsigned _primordial_arena = UINT_MAX;
static __thread bool _joined = false;

static void *_chunk_alloc(size_t size, size_t alignment, bool *zero,
                          unsigned arena) {
  return heap_chunk_alloc(global_heap, size, alignment, zero, arena);
}

static bool _chunk_dalloc(void *chunk, size_t size, unsigned arena) {
  return heap_chunk_dalloc(global_heap, chunk, size, arena);
}

int pgas_join(void) {
  if (!global_heap) {
    dbg_error("pgas: attempt to join GAS before global heap allocation.\n");
    return LIBHPX_ERROR;
  }

  if (_global_arena == UINT_MAX)
    _global_arena = mallctl_create_arena(_chunk_alloc, _chunk_dalloc);

  if (_local_arena == UINT_MAX)
    _local_arena =  mallctl_create_arena(NULL, NULL);

  mallctl_thread_enable_cache();
  mallctl_thread_flush_cache();
  unsigned old = mallctl_thread_set_arena(_global_arena);
  if (_primordial_arena == UINT_MAX)
    _primordial_arena = old;

  _joined = true;

  return LIBHPX_OK;
}

void pgas_leave(void) {
  if (_local_arena == UINT_MAX || _global_arena == UINT_MAX)
    dbg_error("pgas: trying to leave the GAS before joining it.\n");

  mallctl_thread_flush_cache();
  mallctl_thread_set_arena(_local_arena);
  _joined = false;
}

void *pgas_global_malloc(size_t bytes) {
  void *addr = (_joined) ? default_malloc(bytes)
                         : arena_malloc(_global_arena, bytes);

  DEBUG_IF (!lva_is_global(addr))
    dbg_error("global malloc returned local pointer %p.\n", addr);

  dbg_log_gas("%p, %lu\n", addr, bytes);
  return addr;
}

void pgas_global_free(void *ptr) {
  if (!ptr)
    return;

  assert(global_heap);
  dbg_log_gas("%p\n", ptr);

  DEBUG_IF (!lva_is_global(ptr))
    dbg_error("global free called on local pointer %p.\n", ptr);

  if (_joined)
    default_free(ptr);
  else
    arena_free(_global_arena, ptr);
}

void *pgas_global_calloc(size_t nmemb, size_t size) {
  void *addr = (_joined) ? default_calloc(nmemb, size)
                         : arena_calloc(_global_arena, nmemb, size);

  DEBUG_IF (!lva_is_global(addr))
    dbg_error("global calloc returned local pointer %p.\n", addr);

  dbg_log_gas("%p, %lu, %lu\n", addr, nmemb, size);
  return addr;
}

void *pgas_global_realloc(void *ptr, size_t size) {
  DEBUG_IF (ptr && !lva_is_global(ptr))
    dbg_error("global realloc called on local pointer %p.\n", ptr);

  void *addr = (_joined) ? default_realloc(ptr, size)
                         : arena_realloc(_global_arena, ptr, size);

  DEBUG_IF (!lva_is_global(addr))
    dbg_error("global realloc returned local pointer %p.\n", addr);

  dbg_log_gas("%p, %p, %lu\n", addr, ptr, size);
  return addr;
}

void *pgas_global_valloc(size_t size) {
  void *addr = (_joined) ? default_valloc(size)
                         : arena_valloc(_global_arena, size);

  DEBUG_IF (!lva_is_global(addr))
    dbg_error("global valloc returned local pointer %p.\n", addr);

  dbg_log_gas("%p, %lu\n", addr, size);
  return addr;
}

void *pgas_global_memalign(size_t boundary, size_t size) {
  void *addr = (_joined) ? default_memalign(boundary, size)
                         : arena_memalign(_global_arena, boundary, size);

  DEBUG_IF (!lva_is_global(addr))
    dbg_error("global memalign returned local pointer %p.\n", addr);

  dbg_log_gas("%p, %lu, %lu\n", addr, boundary, size);
  return addr;
}

int pgas_global_posix_memalign(void **memptr, size_t alignment, size_t size) {
  int e = (_joined) ? default_posix_memalign(memptr, alignment, size)
                    : arena_posix_memalign(_global_arena, memptr, alignment, size);

  DEBUG_IF (!e && !lva_is_global(*memptr)) {
    dbg_error("global posix memalign returned local pointer %p.\n", *memptr);
  }

  if (!e)
    dbg_log_gas("%d, %p, %lu, %lu\n", e, *memptr, alignment, size);
  return e;
}

void *pgas_local_malloc(size_t bytes) {
  void *addr = (_joined) ? arena_malloc(_local_arena, bytes)
                         : default_malloc(bytes);

  DEBUG_IF (lva_is_global(addr)) {
    dbg_error("local malloc returned global pointer %p.\n", addr);
  }

  dbg_log_gas("%p, %lu\n", addr, bytes);
  return addr;
}

void pgas_local_free(void *ptr) {
  if (!ptr)
    return;

  assert(!heap_contains(global_heap, ptr));
  dbg_log_gas("%p\n", ptr);

  DEBUG_IF (lva_is_global(ptr)) {
    dbg_error("local free passed global pointer %p.\n", ptr);
  }

  if (_joined)
    arena_free(_local_arena, ptr);
  else
    default_free(ptr);
}

void *pgas_local_calloc(size_t nmemb, size_t size) {
  void *addr = (_joined) ? arena_calloc(_local_arena, nmemb, size)
                         : default_calloc(nmemb, size);

  DEBUG_IF (lva_is_global(addr)) {
    dbg_error("local calloc returned global pointer %p.\n", addr);
  }

  dbg_log_gas("%p, %lu, %lu\n", addr, nmemb, size);
  return addr;
}

void *pgas_local_realloc(void *ptr, size_t size) {
  DEBUG_IF (lva_is_global(ptr)) {
    dbg_error("local realloc called on global pointer %p.\n", ptr);
  }

  void *addr = (_joined) ? arena_realloc(_local_arena, ptr, size)
                         : default_realloc(ptr, size);

  DEBUG_IF (lva_is_global(addr)) {
    dbg_error("pgas: local realloc returned global pointer %p.\n", addr);
  }

  dbg_log_gas("%p, %p, %lu\n", addr, ptr, size);
  return addr;
}

void *pgas_local_valloc(size_t size) {
  void *addr = (_joined) ? arena_valloc(_local_arena, size)
                         : default_valloc(size);

  DEBUG_IF (lva_is_global(addr)) {
    dbg_error("pgas: local valloc returned global pointer %p.\n", addr);
  }

  dbg_log_gas("%p, %lu\n", addr, size);
  return addr;
}

void *pgas_local_memalign(size_t boundary, size_t size) {
  void *addr = (_joined) ? arena_memalign(_local_arena, boundary, size)
                         : default_memalign(boundary, size);

  DEBUG_IF (lva_is_global(addr)) {
    dbg_error("local memalign returned global pointer %p.\n", addr);
  }

  dbg_log_gas("%p, %lu, %lu\n", addr, boundary, size);
  return addr;
}

int pgas_local_posix_memalign(void **memptr, size_t alignment, size_t size) {
  int e = (_joined) ? arena_posix_memalign(_local_arena, memptr, alignment, size)
                    : default_posix_memalign(memptr, alignment, size);

  DEBUG_IF (!e && lva_is_global(*memptr)) {
    dbg_error("local posix memalign returned global pointer %p.\n", *memptr);
  }

  if (!e)
    dbg_log_gas("%d, %p, %lu, %lu\n", e, *memptr, alignment, size);
  return e;
}
