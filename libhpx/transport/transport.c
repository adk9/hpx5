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
#include "config.h"
#endif

/// @file libhpx/network/transport/transport.c
/// @brief Handles transport initialization.
#include "libhpx/debug.h"
#include "libhpx/transport.h"

static transport_t *_default(config_t *cfg) {
#ifdef HAVE_PHOTON
  return transport_new_photon(cfg);
#endif

#ifdef HAVE_PORTALS
  return transport_new_portals(cfg);
#endif

#ifdef HAVE_MPI
  return transport_new_mpi(cfg);
#endif

  return transport_new_smp();
}

transport_t *transport_new(hpx_transport_t type, config_t *cfg) {
  transport_t *transport = NULL;

  switch (type) {
   case (HPX_TRANSPORT_PHOTON):
#ifdef HAVE_PHOTON
    transport = transport_new_photon(cfg);
#else
    dbg_error("Photon transport not supported in current configuration.\n");
#endif
    break;

   case (HPX_TRANSPORT_MPI):
#ifdef HAVE_MPI
    transport = transport_new_mpi(cfg);
#else
    dbg_error("MPI transport not supported in current configuration.\n");
#endif
    break;

   case (HPX_TRANSPORT_PORTALS):
#ifdef HAVE_PORTALS
    transport = transport_new_portals(cfg);
#else
    dbg_error("Portals transport not supported in current configuration.\n");
#endif
    break;

   case (HPX_TRANSPORT_DEFAULT):
   default:
    transport = _default(cfg);
    break;
  };

  if (!transport) {
    transport = _default(cfg);
  }

  if (!transport) {
    log_trans("failed to initialize a transport.\n");
  }
  else {
    log("initialized the %s transport.\n", transport->id());
  }

  return transport;
}


