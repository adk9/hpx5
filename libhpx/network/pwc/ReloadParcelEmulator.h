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

#ifndef LIBHPX_NETWORK_PWC_RELOAD_PARCEL_EMULATOR_H
#define LIBHPX_NETWORK_PWC_RELOAD_PARCEL_EMULATOR_H

#include "xport.h"
#include "libhpx/boot.h"
#include "libhpx/config.h"
#include "libhpx/parcel.h"
#include "parcel_emulation.h"

namespace libhpx {
namespace network {
namespace pwc {
class ReloadParcelEmulator {
 public:
  ReloadParcelEmulator(const config_t *cfg, boot_t *boot, pwc_xport_t *xport)
      : impl_(parcel_emulator_new_reload(cfg, boot, xport))
  {
  }

  ~ReloadParcelEmulator()
  {
    if (impl_) {
      impl_->deallocate(impl_);
    }
  }

  int send(pwc_xport_t *xport, unsigned rank, const hpx_parcel_t *p)
  {
    return impl_->send(impl_, xport, rank, p);
  }

  parcel_emulator_t * const impl()
  {
    return impl_;
  }

 private:
  parcel_emulator_t *impl_;
};
}
} // namespace network
} // namespace libhpx

#endif // LIBHPX_NETWORK_PWC_RELOAD_PARCEL_EMULATOR_H
