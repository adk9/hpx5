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

#include "PWCNetwork.h"

namespace {
using libhpx::network::ParcelStringOps;
using libhpx::network::pwc::PWCNetwork;
using libhpx::network::pwc::AGASNetwork;
}

AGASNetwork::AGASNetwork(const config_t *cfg, boot_t *boot, gas_t *gas)
    : PWCNetwork(cfg, boot, gas),
      ParcelStringOps()
{
}

