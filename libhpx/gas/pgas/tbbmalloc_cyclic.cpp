// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#include <cassert>
#include <tbb/scalable_allocator.h>
#include <libhpx/memory.h>
#include "cyclic.h"
#include "heap.h"

using namespace rml;

static void *
_cyclic_chunk_alloc(intptr_t pool_id, size_t &bytes) {
  assert(pool_id == AS_CYCLIC);
  return heap_cyclic_chunk_alloc(global_heap, NULL, bytes,
                                 global_heap->bytes_per_chunk);
}

static int
_cyclic_chunk_free(intptr_t pool_id, void* raw_ptr, size_t raw_bytes) {
  assert(pool_id == AS_CYCLIC);
  heap_chunk_dalloc(global_heap, raw_ptr, raw_bytes);
  return 0;
}

void cyclic_allocator_init(int rank) {
  if (rank) {
    return;
  }

  int id = AS_CYCLIC;
  size_t granularity = as_bytes_per_chunk();
  const MemPoolPolicy policy(_cyclic_chunk_alloc, _cyclic_chunk_free,
                             granularity);

  MemoryPool* pool = NULL;
  pool_create_v1(id, &policy, &pool);
  pools[id] = pool;
}

void cyclic_allocator_fini(void) {
}
