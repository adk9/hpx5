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

/// ----------------------------------------------------------------------------
/// Dummy Controller API.
///
/// ----------------------------------------------------------------------------
#include <stdlib.h>
#include "hpx/hpx.h"
#include "libhpx/debug.h"
#include "libhpx/routing.h"
#include "managers.h"

static void _delete(routing_t *routing) {
}

static int _add_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) {
  return HPX_SUCCESS;
}

static int _delete_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) {
  return HPX_SUCCESS;
}

static int _update_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) {
  return HPX_SUCCESS;
}

static int _my_port(const routing_t *r) {
  return 0;
}

static int _register_addr(const routing_t *r, uint64_t addr) {
  return HPX_SUCCESS;
}

static int _unregister_addr(const routing_t *r, uint64_t addr) {
  return HPX_SUCCESS;
}

static routing_t _routing = {
  .delete          = _delete,
  .add_flow        = _add_flow,
  .delete_flow     = _delete_flow,
  .update_flow     = _update_flow,
  .my_port         = _my_port,
  .register_addr   = _register_addr,
  .unregister_addr = _unregister_addr,
};

routing_t *routing_new_dummy(void) {
  return &_routing;
}
