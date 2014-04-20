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
/// @file libhpx/network/routing/routing.c
/// @brief Handles flow rules and enables intelligent routing in the network.
/// ----------------------------------------------------------------------------
#include "libhpx/routing.h"
#include "libhpx/debug.h"

static routing_class_t *_default(void) {
#ifdef HAVE_FLOODLIGHT
  return routing_new_floodlight();
#endif

#ifdef HAVE_TREMA
  return routing_new_trema();
#endif

  return routing_new_dummy();
}


routing_class_t *routing_new(hpx_routing_t type) {
  routing_class_t *routing = NULL;

  switch (type) {
   case (HPX_ROUTING_TREMA):
#ifdef HAVE_TREMA
    routing = routing_new_trema();
    if (routing) {
      dbg_log("initialized Trema (OpenFlow) routing control.\n");
      return routing;
    }
#else
    dbg_error("Trema routing control not supported in current configuration.\n");
    break;
#endif

   case (HPX_ROUTING_FLOODLIGHT):
#if HAVE_FLOODLIGHT
     routing = routing_new_floodlight();
     if (routing) {
       dbg_log("initialized Floodlight (OpenFlow) routing control.\n");
       return routing;
     }
#else
     dbg_error("Floodlight routing control not supported in current configuration.\n");
     break;
#endif

   case HPX_BOOT_DEFAULT:
   default:
     routing = _default();
     return routing;
  }
  return routing;
}

