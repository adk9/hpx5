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

#ifndef LIBHPX_PARCEL_OPS_H
#define LIBHPX_PARCEL_OPS_H

/// The abstract base class for a provider for parcel operations.

namespace libhpx {
class ParcelOps {
 public:
  virtual ~ParcelOps();
  virtual int send(hpx_parcel_t* p, hpx_parcel_t* ssync) = 0;
  virtual void deallocate(const hpx_parcel_t* p) = 0;
};
}

#endif // LIBHPX_PARCEL_OPS_H
