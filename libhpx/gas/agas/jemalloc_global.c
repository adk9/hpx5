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

/// @file  libhpx/gas/agas/global.c
/// @brief This file implements the address-space allocator interface for global
///        memory management.
///
/// In practice this is trivial. We just use these to bind the heap_chunk
/// allocation routines to the global bitmap instance.
#include <string.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include "agas.h"

static void *
_global_chunk_alloc(void *addr, size_t n, size_t align, bool *z, unsigned arena)
{
  dbg_assert(here && here->gas);
  agas_t *agas = (agas_t*)here->gas;
  dbg_assert(agas->bitmap);
  void *chunk = agas_chunk_alloc(agas, agas->bitmap, addr, n, align);
  if (z && *z && chunk) {
    memset(chunk, 0, n);
  }
  return chunk;
}

static bool
_global_chunk_free(void *addr, size_t n, unsigned arena) {
  dbg_assert(here && here->gas);
  agas_t *agas = (agas_t*)here->gas;
  dbg_assert(agas->bitmap);
  agas_chunk_dalloc(agas, agas->bitmap, addr, n);
  return 0;
}

static bool
_global_chunk_purge(void *addr, size_t offset, size_t size, unsigned arena) {
  log_error("purging global memory is currently unsupported\n");
  return 1;
}

static chunk_allocator_t _agas_global_allocator = {
  .challoc = _global_chunk_alloc,
  .chfree  = _global_chunk_free,
  .chpurge = _global_chunk_purge
};

void
agas_global_allocator_init(agas_t *agas) {
  as_set_allocator(AS_GLOBAL, &_agas_global_allocator);
}
