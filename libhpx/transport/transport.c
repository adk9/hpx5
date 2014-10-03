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

/// ----------------------------------------------------------------------------
/// @file libhpx/network/transport/transport.c
/// @brief Handles transport initialization.
/// ----------------------------------------------------------------------------
#include "libhpx/debug.h"
#include "libhpx/transport.h"

static transport_class_t *_default(uint32_t req_limit) {
#ifdef HAVE_PHOTON
  return transport_new_photon(req_limit);
#endif

#ifdef HAVE_PORTALS
  return transport_new_portals(req_limit);
#endif

#ifdef HAVE_MPI
  return transport_new_mpi(req_limit);
#endif

  return transport_new_smp(req_limit);
}

transport_class_t *transport_new(hpx_transport_t transport, uint32_t req_limit) {
  switch (transport) {
   case (HPX_TRANSPORT_PHOTON):
#ifdef HAVE_PHOTON
    return transport_new_photon(req_limit);
#else
    dbg_error("Photon transport not supported in current configuration.\n");
    break;
#endif

   case (HPX_TRANSPORT_MPI):
#ifdef HAVE_MPI
    return transport_new_mpi(req_limit);
#else
    dbg_error("MPI transport not supported in current configuration.\n");
    break;
#endif

   case (HPX_TRANSPORT_PORTALS):
#ifdef HAVE_PORTALS
    return transport_new_portals(req_limit);
#else
    dbg_error("Portals transport not supported in current configuration.\n");
    break;
#endif

   case (HPX_TRANSPORT_SMP):
    return transport_new_smp(req_limit);

   case (HPX_TRANSPORT_DEFAULT):
   default:
    break;
  };

  return _default(req_limit);
}


