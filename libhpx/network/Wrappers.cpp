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

#include "Wrappers.h"

namespace {
using libhpx::Network;
using libhpx::network::NetworkWrapper;
}

NetworkWrapper::NetworkWrapper(Network *impl)
    : Network(),
      impl_(impl)
{
}

libhpx::network::NetworkWrapper::~NetworkWrapper()
{
  delete impl_;
}
