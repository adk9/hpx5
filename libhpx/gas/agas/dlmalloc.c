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

#include <string.h>
#include <libhpx/debug.h>
#include <libhpx/memory.h>
#include <malloc-2.8.6.h>
#include "cyclic.h"
#include "global.h"
#include "heap.h"

void
_agas_allocator_init(agas_t *agas, int id, size_t bytes) {
  dbg_assert(agas);
  void *base = agas_chunk_alloc(agas, agas->bitmap, NULL, bytes,
                                agas->chunk_size);
  if (base) {
    memset(base, 0, bytes);
  }
  mspaces[AS_GLOBAL] = create_mspace_with_base(base, bytes, 0);
}

void
agas_cyclic_allocator_init(agas_t *agas) {
  size_t bytes = ceil_div_size_t(cfg->heapsize, 2);
  _agas_allocator_init(agas, AS_CYCLIC, 0);
}

void
agas_global_allocator_init(agas_t *agas) {
  size_t bytes = ceil_div_size_t(cfg->heapsize, 2);
  _agas_allocator_init(agas, AS_GLOBAL, 0);
}

