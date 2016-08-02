// ==================================================================-*- C++ -*-
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

#ifndef LIBHPX_MEMORY_OPS_H
#define LIBHPX_MEMORY_OPS_H

/// The abstract base class for a provider for memory operations.

namespace libhpx {
class MemoryOps {
 public:
  virtual ~MemoryOps();
  virtual void pin(const void *base, size_t bytes, void *key) = 0;
  virtual void unpin(const void *base, size_t bytes) = 0;
};
}

#endif // LIBHPX_MEMORY_OPS_H
