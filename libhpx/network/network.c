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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/// @file libhpx/network/network.c
#include <libhpx/network.h>
#include "isir/isir.h"

network_t *network_new(libhpx_network_t type, struct gas_class *gas, int nrx) {
  // return _old_new(nrx);
  return network_isir_funneled_new(gas, nrx);
}
