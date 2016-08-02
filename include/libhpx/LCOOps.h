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

#ifndef LIBHPX_LCO_OPS_H
#define LIBHPX_LCO_OPS_H

/// The abstract base class for a provider for LCO operations.
#include "hpx/hpx.h"

namespace libhpx {
class LCOOps {
 public:
  virtual ~LCOOps();
  virtual int wait(hpx_addr_t lco, int reset) = 0;
  virtual int get(hpx_addr_t lco, size_t n, void *to, int reset) = 0;
};
}

#endif // LIBHPX_LCO_OPS_H
