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
#include <hpx/hpx.h>
#include "libhpx/libhpx.h"
#include "libhpx/debug.h"
#include "libhpx/gas.h"
#include "../bitmap.h"
#include "../malloc.h"
#include "../mallctl.h"
#include "heap.h"


/// The PGAS type is a global address space that manages a shared heap.
static heap_t *_global_heap = NULL;

static __thread unsigned _global_arena = UINT_MAX;
static __thread unsigned _local_arena = UINT_MAX;
static __thread unsigned _primordial_arena = UINT_MAX;
static __thread bool _joined = false;

static void *_chunk_alloc(size_t size, size_t alignment, bool *zero, unsigned arena) {
  return heap_chunk_alloc(_global_heap, size, alignment, zero, arena);
}

static bool _chunk_dalloc(void *chunk, size_t size, unsigned arena) {
  return heap_chunk_dalloc(_global_heap, chunk, size, arena);
}

static int _pgas_join(void) {
  if (!_global_heap) {
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

static void _pgas_leave(void) {
  if (_local_arena == UINT_MAX || _global_arena == UINT_MAX)
    dbg_error("pgas: trying to leave the GAS before joining it.\n");

  mallctl_thread_flush_cache();
  mallctl_thread_set_arena(_local_arena);
  _joined = false;
}

static void *_pgas_global_malloc(size_t bytes) {
  return (_joined) ? libhpx_malloc(bytes)
                   : arena_malloc(_global_arena, bytes);
}

static void _pgas_global_free(void *ptr) {
  if (!ptr)
    return;

  assert(heap_contains(_global_heap, ptr));

  if (_joined)
    libhpx_free(ptr);
  else
    arena_free(_global_arena, ptr);
}

static void *_pgas_global_calloc(size_t nmemb, size_t size) {
  return (_joined) ? libhpx_calloc(nmemb, size)
                   : arena_calloc(_global_arena, nmemb, size);
}

static void *_pgas_global_realloc(void *ptr, size_t size) {
  return (_joined) ? libhpx_realloc(ptr, size)
                   : arena_realloc(_global_arena, ptr, size);
}

static void *_pgas_global_valloc(size_t size) {
  return (_joined) ? libhpx_valloc(size)
                   : arena_valloc(_global_arena, size);
}

static void *_pgas_global_memalign(size_t boundary, size_t size) {
  return (_joined) ? libhpx_memalign(boundary, size)
                   : arena_memalign(_global_arena, boundary, size);
}

static int _pgas_global_posix_memalign(void **memptr, size_t alignment,
                                      size_t size) {
  return (_joined) ? libhpx_posix_memalign(memptr, alignment, size)
                   : arena_posix_memalign(_global_arena, memptr, alignment, size);
}

static void *_pgas_local_malloc(size_t bytes) {
  return (_joined) ? arena_malloc(_local_arena, bytes)
                   : libhpx_malloc(bytes);
}

static void _pgas_local_free(void *ptr) {
  if (!ptr)
    return;

  assert(!heap_contains(_global_heap, ptr));

  if (_joined)
    arena_free(_local_arena, ptr);
  else
    libhpx_free(ptr);
}

static void *_pgas_local_calloc(size_t nmemb, size_t size) {
  return (_joined) ? arena_calloc(_local_arena, nmemb, size)
                   : libhpx_calloc(nmemb, size);
}

static void *_pgas_local_realloc(void *ptr, size_t size) {
  return (_joined) ? arena_realloc(_local_arena, ptr, size)
                   : libhpx_realloc(ptr, size);
}

static void *_pgas_local_valloc(size_t size) {
  return (_joined) ? arena_valloc(_local_arena, size)
                   : libhpx_valloc(size);
}

static void *_pgas_local_memalign(size_t boundary, size_t size) {
  return (_joined) ? arena_memalign(_local_arena, boundary, size)
                   : libhpx_memalign(boundary, size);
}

static int _pgas_local_posix_memalign(void **memptr, size_t alignment,
                                      size_t size) {
  return (_joined) ? arena_posix_memalign(_local_arena, memptr, alignment, size)
                   : libhpx_posix_memalign(memptr, alignment, size);
}

static void _pgas_delete(gas_class_t *gas) {
  if (_global_heap) {
    heap_fini(_global_heap);
    free(_global_heap);
    _global_heap = NULL;
  }
}

static gas_class_t _pgas_vtable = {
  .type   = HPX_GAS_PGAS,
  .delete = _pgas_delete,
  .join   = _pgas_join,
  .leave  = _pgas_leave,
  .global = {
    .malloc         = _pgas_global_malloc,
    .free           = _pgas_global_free,
    .calloc         = _pgas_global_calloc,
    .realloc        = _pgas_global_realloc,
    .valloc         = _pgas_global_valloc,
    .memalign       = _pgas_global_memalign,
    .posix_memalign = _pgas_global_posix_memalign
  },
  .local  = {
    .malloc         = _pgas_local_malloc,
    .free           = _pgas_local_free,
    .calloc         = _pgas_local_calloc,
    .realloc        = _pgas_local_realloc,
    .valloc         = _pgas_local_valloc,
    .memalign       = _pgas_local_memalign,
    .posix_memalign = _pgas_local_posix_memalign
  }
};

gas_class_t *gas_pgas_new(size_t heap_size) {
  if (_global_heap)
    return &_pgas_vtable;

  if (!mallctl_disable_dirty_page_purge()) {
    dbg_error("pgas: failed to disable dirty page purging\n");
    return NULL;
  }

  _global_heap = malloc(sizeof(*_global_heap));
  if (!_global_heap) {
    dbg_error("pgas: could not allocate global heap\n");
    return NULL;
  }

  int e = heap_init(_global_heap, heap_size);
  if (e) {
    dbg_error("pgas: failed to allocate global heap\n");
    free(_global_heap);
    return NULL;
  }

  return &_pgas_vtable;
}
