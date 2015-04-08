// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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
#include <libhpx/boot.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/network.h>
#include "isir/isir.h"
#include "pwc/pwc.h"
#include "smp.h"

static network_t *_default(const config_t *cfg, struct boot *boot,
                           struct gas *gas) {
  network_t *network = NULL;
  if (boot_n_ranks(boot) == 1) {
    network = network_smp_new(cfg, boot);
    if (network) {
      return network;
    }
  }

#if defined(ENABLE_PWC) && defined(HAVE_PHOTON)
  network = network_pwc_funneled_new(cfg, boot, gas);
  if (network) {
    return network;
  }
#endif

#ifdef HAVE_MPI
  network =  network_isir_funneled_new(cfg, boot, gas);
  if (network) {
    return network;
  }
#endif

  network = network_smp_new(cfg, boot);
  return network;
}

network_t *network_new(const config_t *cfg, struct boot *boot, struct gas *gas)
{
  network_t *network = NULL;

  switch (cfg->network) {
   case HPX_NETWORK_PWC:
#if defined(ENABLE_PWC) && defined(HAVE_NETWORK)
    network = network_pwc_funneled_new(cfg, boot, gas);
#else
    log_cfg("network support not enabled\n");
#endif
    break;

   case HPX_NETWORK_ISIR:
#ifdef HAVE_NETWORK
    network = network_isir_funneled_new(cfg, boot, gas);
#else
    log_cfg("network support not enabled\n");
#endif
    break;

   case HPX_NETWORK_SMP:
    network = network_smp_new(cfg, boot);
    break;

   default:
    network = _default(cfg, boot, gas);
    break;
  }

  if (!network && (cfg->network == HPX_NETWORK_DEFAULT)) {
    network = _default(cfg, boot, gas);
  }

  if (!network) {
    dbg_error("failed to initialize the network\n");
  }
  else {
    log("network initialized using %s\n", HPX_NETWORK_TO_STRING[network->type]);
  }

  return network;
}
