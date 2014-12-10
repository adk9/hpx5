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

#include <libhpx/debug.h>
#include <libhpx/network.h>
#include "isir/isir.h"
#include "pwc/pwc.h"
#include "smp.h"

static network_t *_default(struct boot *boot, struct gas *gas, int nrx) {
  network_t *network = NULL;
#ifdef HAVE_PHOTON
  network = network_pwc_funneled_new(boot, gas, nrx);
  if (network) {
    return network;
  }
#endif

#ifdef HAVE_MPI
  network = network_isir_funneled_new(gas, nrx);
  if (network) {
    return network;
  }
#endif

  network = network_smp_new();
  return network;
}


network_t *network_new(libhpx_network_t type, struct boot *boot,
                       struct gas *gas, int nrx)
{
  // return _old_new(nrx);
  network_t *network = NULL;

  switch (type) {
   case LIBHPX_NETWORK_PHOTON:
    network = network_pwc_funneled_new(boot, gas, nrx);
    break;
   case LIBHPX_NETWORK_MPI:
    network = network_isir_funneled_new(gas, nrx);
    break;
   default:
    network = _default(boot, gas, nrx);
    break;
  }

  if (!network) {
    network = _default(boot, gas, nrx);
  }

  if (!network) {
    dbg_error("failed to initialize the network\n");
  }
  else {
    dbg_log("network initialized using %s.", network->id());
  }

  return network;
}
