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


transport_class_t *transport_new(hpx_transport_t transport) {
  transport_class_t *t = NULL;
  switch (transport) {
   case (HPX_TRANSPORT_PHOTON):
#ifdef HAVE_PHOTON
    if ((t = transport_new_photon(void)) == NULL)
      dbg_error("Photon transport failed to initialize.\n");
#else
    dbg_error("Photon transport not supported in current configuration.\n");
#endif
    break;

   case (HPX_TRANSPORT_MPI):
#ifdef HAVE_MPI
    if ((t = transport_new_mpi()) == NULL)
      dbg_error("MPI transport failed to initialize.\n");
#else
    dbg_error("MPI transport not supported in current configuration.\n");
#endif
    break;

   case (HPX_TRANSPORT_PORTALS):
#ifdef HAVE_PORTALS
    if ((t = transport_new_portals()) == NULL)
      dbg_error("Portals transport failed to initialize.\n");
#else
    dbg_error("Portals transport not supported in current configuration.\n");
#endif
    break;

   default:
   case (HPX_TRANSPORT_SMP):
    t = transport_new_smp();
    break;
  };

  if (!t)
    t = transport_new_smp();

  if (!t) {
    dbg_error("Failed to initialize transport.\n");
    return NULL;
  }

  dbg_log("initialized the %s transport.\n" t->id());
  return t;
}


