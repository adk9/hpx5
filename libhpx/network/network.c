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
#include "libhpx/config.h"
#include "libhpx/debug.h"
#include "libhpx/network.h"
#include "libhpx/transport.h"
#include "isir/isir.h"
#include "pwc/pwc.h"
#include "smp.h"

static network_t *_default(config_t *cfg, struct boot *boot, struct gas *gas,
                           int nrx) {
#ifdef HAVE_PHOTON
  return network_pwc_funneled_new(cfg, boot, gas, nrx);
#endif

#ifdef HAVE_MPI
  return network_isir_funneled_new(cfg, gas, nrx);
#endif
  
  return network_smp_new();
}

int network_supported_transport(transport_t *t, const int tports[], int n) {
  int i;
  for (i=0; i<n; i++) {
    if (t->type == tports[i]) {
      return 0;
    }
  }
  return 1;
}

network_t *network_new(config_t *cfg, struct boot *boot, struct gas *gas,
                       int nrx) {
  network_t *network = NULL;
  
  switch (cfg->network) {
   case HPX_NETWORK_PWC:
#ifdef HAVE_PHOTON
    network = network_pwc_funneled_new(cfg, boot, gas, nrx);
#else
    dbg_error("PWC network not supported in current configuration.\n");
#endif
    break;
   case HPX_NETWORK_ISIR:
#ifdef HAVE_MPI
    network = network_isir_funneled_new(cfg, gas, nrx);
#else
    dbg_error("ISIR network not supported in current configuration.\n");
#endif
    break;
   case HPX_NETWORK_SMP:
    network = network_smp_new();
    break;
   default:
    network = _default(cfg, boot, gas, nrx);
    break;
  }
    
  if (!network && (cfg->network == HPX_NETWORK_DEFAULT)) {
    network = _default(cfg, boot, gas, nrx);
  }
  
  if (!network) {
    dbg_error("failed to initialize the network\n");
  }
  else {
    log("network initialized using %s\n", HPX_NETWORK_TO_STRING[network->type]);
  }

  return network;
}
