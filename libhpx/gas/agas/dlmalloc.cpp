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

#include "AGAS.h"
#include "libhpx/config.h"
#include "libhpx/locality.h"
#include "libhpx/memory.h"
#include "libhpx/util/math.h"
#include <malloc-2.8.6.h>

namespace {
using libhpx::util::ceil_div;
using libhpx::gas::agas::AGAS;
}

static void
_init_allocator(int id)
{
  dbg_assert(id < AS_COUNT);
  size_t bytes = ceil_div(here->config->heapsize, size_t(2));
  size_t align = as_bytes_per_chunk();
  bool cyclic = (id == AS_CYCLIC);
  void *base = AGAS::Instance()->chunkAllocate(nullptr, bytes, align, cyclic);
  dbg_assert(base);
  mspaces[id] = create_mspace_with_base(base, bytes, 1);
}

void
AGAS::initAllocators(unsigned rank)
{
  if (rank == 0) {
    _init_allocator(AS_CYCLIC);
  }
  _init_allocator(AS_GLOBAL);
}
