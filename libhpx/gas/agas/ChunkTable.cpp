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

#include "ChunkTable.h"
#include "libhpx/memory.h"

namespace {
using libhpx::gas::agas::ChunkTable;
}

ChunkTable::ChunkTable(size_t size)
    : chunkMask_(~(as_bytes_per_chunk() - 1)),
      map_(size)
{
}

ChunkTable::~ChunkTable()
{
}


uint64_t
ChunkTable::offsetOf(const void *lva) const
{
  const uintptr_t masked = reinterpret_cast<uintptr_t>(lva) & chunkMask_;
  const uintptr_t offset = reinterpret_cast<uintptr_t>(lva) & ~chunkMask_;
  const void* chunk = reinterpret_cast<void*>(masked);
  uint64_t base;
  const bool found = map_.find(chunk, base);
  assert(found);
  return base + offset;
}

void
ChunkTable::insert(const void *chunk, uint64_t base)
{
  bool inserted = map_.insert(chunk, base);
  assert(inserted);
  (void)inserted;
}

void
ChunkTable::remove(const void *chunk)
{
  bool erased = map_.erase(chunk);
  assert(erased);
  (void)erased;
}

