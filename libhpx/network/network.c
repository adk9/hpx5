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
/// @file libhpx/libhpx/network.c
/// @brief Manages the HPX network.
///
/// This file deals with the complexities of the HPX network interface,
/// shielding it from the details of the underlying transport interface.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdio.h>
#include "network.h"
#include "scheduler.h"
#include "locality.h"
#include "transport.h"
#include "debug.h"

const hpx_addr_t HPX_NULL = { NULL, -1 };
static transport_t *_transport = NULL;

static hpx_action_t _network_free = 0;

static int _network_free_action(void *args);

static void HPX_CONSTRUCTOR _register_actions(void) {
  _network_free = hpx_register_action("_network_free", _network_free_action);
}

int
network_startup(const hpx_config_t *config) {
  _transport = transport_new();
  return (_transport != NULL) ? HPX_SUCCESS : 1;
}

void
network_shutdown(void) {
  _transport->delete(_transport);
}

void
hpx_parcel_send(hpx_parcel_t *p) {
  if (network_addr_is_local(p->target, NULL))
    scheduler_spawn(p);
  else
    UNIMPLEMENTED();
}

void
hpx_parcel_send_sync(hpx_parcel_t *p) {
}

void
network_berrier(void) {
}

bool
network_addr_is_local(hpx_addr_t addr, void **out) {
  if (addr.rank == -1) {
    if (out)
      *out = NULL;

    return true;
  }

  if (addr.rank != hpx_get_my_rank())
    return false;

  if (out)
    *out = addr.local;

  return true;
}

hpx_addr_t
network_malloc(int size, int alignment) {
  hpx_addr_t addr = { NULL, hpx_get_my_rank() };
  int e = posix_memalign(&addr.local, alignment, size);
  if (e) {
    fprintf(stderr, "failed global allocation\n");
    abort();
  }
  return addr;
}

static void _free(hpx_addr_t addr) {
  free(addr.local);
}

int
_network_free_action(void *args) {
  hpx_parcel_t *parcel = scheduler_current_parcel();
  _free(parcel->target);
  return HPX_SUCCESS;
}

void
network_free(hpx_addr_t addr) {
  if (addr.rank == hpx_get_my_rank() )
    _free(addr);
  hpx_call(addr, _network_free, NULL, 0, HPX_NULL);
}

void
network_release(hpx_parcel_t *parcel) {
  free(parcel);
}

bool
hpx_addr_eq(hpx_addr_t lhs, hpx_addr_t rhs) {
  return (lhs.rank == rhs.rank) && (lhs.local == rhs.local);
}

hpx_addr_t
hpx_addr_from_rank(int rank) {
  hpx_addr_t r = { NULL, rank };
  return r;
}

int
hpx_addr_to_rank(hpx_addr_t addr) {
  return addr.rank;
}
