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
/// @file network.c
/// @brief Manages the HPX network.
///
/// This file deals with the complexities of the HPX network interface,
/// shielding it from the
/// ----------------------------------------------------------------------------
#include "network.h"
#include "networks.h"

static network_t *network = NULL;

int
network_init(void) {
  network = smp_new();
  return HPX_SUCCESS;
}

int
network_init_thread(void) {
  return HPX_SUCCESS;
}

void
network_fini(void) {
  smp_delete(network);
}

void
network_fini_thread(void) {
}

void
hpx_parcel_send(hpx_parcel_t *p) {
}

void
hpx_parcel_send_sync(hpx_parcel_t *p) {
}

void
network_berrier(void) {
}

bool
network_addr_is_local(hpx_addr_t addr, void **out) {
  return false;
}
