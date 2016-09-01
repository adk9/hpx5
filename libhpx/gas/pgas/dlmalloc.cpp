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

#include "Allocator.h"
#include "HeapSegment.h"
#include "libhpx/memory.h"
#include "libhpx/util/math.h"
#include <malloc-2.8.6.h>

namespace {
using HeapSegment = libhpx::gas::pgas::HeapSegment;
using GlobalAllocator = libhpx::gas::pgas::GlobalAllocator;
using CyclicAllocator = libhpx::gas::pgas::CyclicAllocator;
using libhpx::util::ceil_div;
}

CyclicAllocator::CyclicAllocator(int rank)
{
  if (!rank) {
    HeapSegment* segment = HeapSegment::Instance();
    size_t bytes = ceil_div(segment->getNBytes(), size_t(2));
    segment->setCsbrk(bytes);
    mspaces[AS_CYCLIC] = create_mspace_with_base(segment->getBase(), bytes, 1);
  }
}

CyclicAllocator::~CyclicAllocator()
{
  // @todo cleanup dlmalloc here
}

GlobalAllocator::GlobalAllocator(int rank)
{
  HeapSegment* segment = HeapSegment::Instance();
  size_t offset = ceil_div(segment->getNBytes(), size_t(2));
  size_t bytes = segment->getNBytes()- - offset;
  void* base = static_cast<char*>(segment->getBase()) + offset;
  mspaces[AS_GLOBAL] = create_mspace_with_base(base, bytes, 1);
}

GlobalAllocator::~GlobalAllocator()
{
}
