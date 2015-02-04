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
#include "config.h"
#endif

/// @file libhpx/network/transport/transport.c
/// @brief Handles transport initialization.
#include "libhpx/debug.h"
#include "libhpx/transport.h"

static transport_t *_default(uint32_t send_limit, uint32_t recv_limit) {
#ifdef HAVE_PHOTON
  return transport_new_photon(send_limit, recv_limit);
#endif

#ifdef HAVE_PORTALS
  return transport_new_portals(send_limit, recv_limit);
#endif

#ifdef HAVE_MPI
  return transport_new_mpi(send_limit, recv_limit);
#endif

  return transport_new_smp();
}

transport_t *transport_new(hpx_transport_t type, uint32_t slim, uint32_t rlim) {
  transport_t *transport = NULL;

  switch (type) {
   case (HPX_TRANSPORT_PHOTON):
#ifdef HAVE_PHOTON
    transport = transport_new_photon(slim, rlim);
#else
    dbg_error("Photon transport not supported in current configuration.\n");
#endif
    break;

   case (HPX_TRANSPORT_MPI):
#ifdef HAVE_MPI
    transport = transport_new_mpi(slim, rlim);
#else
    dbg_error("MPI transport not supported in current configuration.\n");
#endif
    break;

   case (HPX_TRANSPORT_PORTALS):
#ifdef HAVE_PORTALS
    transport = transport_new_portals(slim, rlim);
#else
    dbg_error("Portals transport not supported in current configuration.\n");
#endif
    break;

   case (HPX_TRANSPORT_SMP):
    transport = transport_new_smp();
    break;

   case (HPX_TRANSPORT_DEFAULT):
   default:
    transport = _default(slim, rlim);
    break;
  };

  if (!transport) {
    transport = _default(slim, rlim);
  }

  if (!transport) {
    log_trans("failed to initialize a transport.\n");
  }
  else {
    log("initialized the %s transport.\n", transport->id());
  }

  return transport;
}


