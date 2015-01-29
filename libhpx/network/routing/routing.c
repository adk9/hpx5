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
#include "managers.h"

routing_t *routing_new(void) {
  routing_t *routing = NULL;

#ifdef HAVE_TREMA
  routing = routing_new_trema();
  if (routing) {
    log_net("initialized Trema (OpenFlow) routing control.\n");
    return routing;
  }
#endif

#if HAVE_FLOODLIGHT
  routing = routing_new_floodlight();
  if (routing) {
    log_net("initialized Floodlight (OpenFlow) routing control.\n");
    return routing;
  }
#endif

  routing = routing_new_dummy();
  return routing;
}

void routing_delete(routing_t *routing) {
  routing->delete(routing);
}

int routing_add_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) {
  return r->add_flow(r, src, dst, port);
}

int routing_delete_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) {
  return r->delete_flow(r, src, dst, port);
}

int routing_update_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) {
  return r->update_flow(r, src, dst, port);
}

int routing_my_port(const routing_t *r) {
  return r->my_port(r);
}

int routing_register_addr(const routing_t *r, uint64_t addr) {
  return r->register_addr(r, addr);
}

int routing_unregister_addr(const routing_t *r, uint64_t addr) {
  return r->unregister_addr(r, addr);
}
