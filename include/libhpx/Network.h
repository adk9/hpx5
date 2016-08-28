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

#ifndef LIBHPX_NETWORK_CXX_H
#define LIBHPX_NETWORK_CXX_H

#include "libhpx/boot.h"
#include "libhpx/CollectiveOps.h"
#include "libhpx/config.h"
#include "libhpx/gas.h"
#include "libhpx/LCOOps.h"
#include "libhpx/MemoryOps.h"
#include "libhpx/ParcelOps.h"
#include "libhpx/StringOps.h"

namespace libhpx {

class Network {
 public:
  virtual ~Network();

  /// Create a new network.
  ///
  /// @param          cfg The current configuration.
  /// @param         boot The bootstrap network object.
  /// @param          gas The global address space.
  ///
  /// @returns            The network object, or NULL if there was an issue.
  static Network* Create(config_t *config, boot_t *boot, gas_t *gas);

  /// Get the HPX configuration type of the base network implementation.
  virtual int type() const = 0;

  /// Progress the network.
  virtual void progress(int n) = 0;

  /// Flush the network.
  virtual void flush() = 0;

  /// Probe the network for received parcels.
  virtual hpx_parcel_t* probe(int rank) = 0;

  virtual CollectiveOps& collectiveOpsProvider() = 0;
  virtual LCOOps& lcoOpsProvider() = 0;
  virtual MemoryOps& memoryOpsProvider() = 0;
  virtual ParcelOps& parcelOpsProvider() = 0;
  virtual StringOps& stringOpsProvider() = 0;
};

} // namespace libhpx

#endif // LIBHPX_NETWORK_CXX_H
