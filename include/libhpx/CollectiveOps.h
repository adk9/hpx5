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

#ifndef LIBHPX_COLLECTIVE_OPS_H
#define LIBHPX_COLLECTIVE_OPS_H

/// The abstract base class for a provider for COLLECTIVE operations.

#include <cstddef>
#include <libhpx/collective.h>

namespace libhpx {
class CollectiveOps {
 public:
  virtual ~CollectiveOps();
  virtual int coll_init(coll_t **collective) = 0;
  //TODO fix sync interface
  virtual int coll_sync(void *in, size_t in_size, void* out, void *collective) = 0;
  virtual int coll_async(coll_data_t *dt, coll_t* c, hpx_addr_t lsync, hpx_addr_t rsync) = 0 ;
};
}

#endif // LIBHPX_COLLECTIVE_OPS_H
