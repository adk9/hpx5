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
#include "../mallctl.h"
#include "heap.h"


/// The PGAS type is a global address space that manages a shared heap.
typedef struct {
  gas_class_t vtable;
  heap_t heap;
} _pgas_t;


/// Threads join PGAS by swapping their arena into private space with an arena
/// into the shared heap. This thread-local variable remembers the private space
/// so that we can continue to use it to satisfy private allocations.
static __thread unsigned _pvt_arena = UINT_MAX;
static __thread int _pvt_flags = 0;


/// Threads remember the PGAS instance that they join.
static __thread _pgas_t *_pgas = NULL;

static void *_chunk_alloc(size_t size, size_t alignment, bool *zero,
                          unsigned arena) {
  if (!_pgas) {
    dbg_error("pgas: gas alloc requires thread to join the GAS instance.\n");
    return NULL;
  }
  return heap_chunk_alloc(&_pgas->heap, size, alignment, zero, arena);
}

static bool _chunk_dalloc(void *chunk, size_t size, unsigned arena) {
  assert(_pgas);
  return heap_chunk_dalloc(&_pgas->heap, chunk, size, arena);
}

static void _pgas_delete(gas_class_t *gas) {
  _pgas_t *pgas = (void*)gas;

  if (!pgas)
    return;

  heap_fini(&pgas->heap);
  free(pgas);
}

static int _pgas_join(gas_class_t *gas) {
  if (_pgas)
    return LIBHPX_OK;

  unsigned arena = mallctl_create_arena(_chunk_alloc, _chunk_dalloc);
  mallctl_thread_enable_cache();
  mallctl_thread_flush_cache();
  _pvt_arena = mallctl_thread_set_arena(arena);
  _pvt_flags = MALLOCX_ARENA(_pvt_arena);
  _pgas = (void*)gas;
  return LIBHPX_OK;
}

static void _pgas_leave(gas_class_t *gas) {
  /// @todo: should get rid of my private arena or something
}

static void *_pgas_malloc(gas_class_t *gas, size_t bytes) {
  if (!bytes)
    return NULL;

  assert(_pgas && gas == (void*)_pgas);
  return mallocx(bytes, 0);
}

static void _pgas_free(gas_class_t *gas, void *ptr) {
  if (!ptr)
    return;

  assert(_pgas && gas == (void*)_pgas);
  dallocx(ptr, 0);
}

static void *_pgas_calloc(gas_class_t *gas, size_t nmemb, size_t size) {
  if (!nmemb || !size)
    return NULL;

  assert(_pgas && gas == (void*)_pgas);
  return mallocx(nmemb * size, MALLOCX_ZERO);
}

static void *_pgas_realloc(gas_class_t *gas, void *ptr, size_t size) {
  if (!ptr)
    return _pgas_malloc(gas, size);

  assert(_pgas && gas == (void*)_pgas);
  return rallocx(ptr, size, 0);
}

static void *_pgas_valloc(gas_class_t *gas, size_t size) {
  if (!size)
    return NULL;

  assert(_pgas && gas == (void*)_pgas);
  return mallocx(size, MALLOCX_ALIGN(HPX_PAGE_SIZE));
}

static void *_pgas_memalign(gas_class_t *gas, size_t boundary, size_t size) {
  if (!size || !boundary)
    return NULL;

  assert(_pgas && gas == (void*)_pgas);
  return mallocx(size, MALLOCX_ALIGN(boundary));
}

static int _pgas_posix_memalign(gas_class_t *gas, void **memptr,
                                size_t alignment, size_t size) {
  if (!size || !alignment) {
    *memptr = NULL;
    return 0;
  }

  assert(_pgas && gas == (void*)_pgas);
  *memptr = mallocx(size, MALLOCX_ALIGN(alignment));
  return (*memptr == 0) ? ENOMEM : 0;
}

static void *_pgas_local_malloc(gas_class_t *gas, size_t bytes) {
  return (bytes) ? mallocx(bytes, _pvt_flags) : NULL;
}

static void _pgas_local_free(gas_class_t *gas, void *ptr) {
  if (ptr)
    dallocx(ptr, _pvt_flags);
}

static void *_pgas_local_calloc(gas_class_t *gas, size_t nmemb, size_t size) {
  return (nmemb && size) ? mallocx(nmemb * size, _pvt_flags | MALLOCX_ZERO) :
                           NULL;
}

static void *_pgas_local_realloc(gas_class_t *gas, void *ptr, size_t size) {
  return (ptr) ? rallocx(ptr, size, _pvt_flags) : _pgas_local_malloc(gas, size);
}

static void *_pgas_local_valloc(gas_class_t *gas, size_t size) {
  return mallocx(size, _pvt_flags | MALLOCX_ALIGN(HPX_PAGE_SIZE));
}

static void *_pgas_local_memalign(gas_class_t *gas, size_t boundary,
                                  size_t size) {
  return mallocx(size, _pvt_flags | MALLOCX_ALIGN(boundary));
}

static int _pgas_local_posix_memalign(gas_class_t *gas, void **memptr,
                                      size_t alignment, size_t size) {
  if (!size || !alignment) {
    *memptr = NULL;
    return 0;
  }

  *memptr = mallocx(size, _pvt_flags | MALLOCX_ALIGN(alignment));
  return (*memptr == 0) ? ENOMEM : 0;
}


gas_class_t *gas_pgas_new(size_t heap_size) {
  if (!mallctl_get_lg_dirty_mult()) {
    dbg_error("HPX requires \"lg_dirty_mult:-1\" set in the environment "
              "variable MALLOC_CONF\n");
    return NULL;
  }

  _pgas_t *pgas = malloc(sizeof(*pgas));        // :-)
  if (!pgas) {
    dbg_error("pgas: could not allocate pgas instance.\n");
    return NULL;
  }

  pgas->vtable.delete               = _pgas_delete;
  pgas->vtable.join                 = _pgas_join;
  pgas->vtable.leave                = _pgas_leave;

  pgas->vtable.malloc               = _pgas_malloc;
  pgas->vtable.free                 = _pgas_free;
  pgas->vtable.calloc               = _pgas_calloc;
  pgas->vtable.realloc              = _pgas_realloc;
  pgas->vtable.valloc               = _pgas_valloc;
  pgas->vtable.memalign             = _pgas_memalign;
  pgas->vtable.posix_memalign       = _pgas_posix_memalign;

  pgas->vtable.local_malloc         = _pgas_local_malloc;
  pgas->vtable.local_free           = _pgas_local_free;
  pgas->vtable.local_calloc         = _pgas_local_calloc;
  pgas->vtable.local_realloc        = _pgas_local_realloc;
  pgas->vtable.local_valloc         = _pgas_local_valloc;
  pgas->vtable.local_memalign       = _pgas_local_memalign;
  pgas->vtable.local_posix_memalign = _pgas_local_posix_memalign;

  int e = heap_init(&pgas->heap, heap_size);
  if (e) {
    dbg_error("pgas: could not initialize heap structure.\n");
    free(pgas);
    return NULL;
  }

  return (void*)pgas;
}
