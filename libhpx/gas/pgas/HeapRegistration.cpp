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

#include "pgas.h"
#include "heap.h"
#include "libhpx/MemoryOps.h"

void*
pgas_pinHeap(void *gas, void *memory_ops, void *key)
{
  auto memops = static_cast<libhpx::MemoryOps*>(memory_ops);
  memops->pin(global_heap->base, global_heap->nbytes, key);
  return global_heap->base;
}

/// Unpin the heap.
///
/// @param          gas The gas object.
/// @param   memory_ops The memory ops provider.
void
pgas_unpinHeap(void *gas, void *memory_ops)
{
  auto memops = static_cast<libhpx::MemoryOps*>(memory_ops);
  memops->unpin(global_heap->base, global_heap->nbytes);
}
