// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#include "libhpx/network.h"
#include "heavy.h"

/// ----------------------------------------------------------------------------
///
/// ----------------------------------------------------------------------------
void* heavy_network(void *args) {
  network_t *network = args;

  while (1) {
    network_progress(network);
  }
}

