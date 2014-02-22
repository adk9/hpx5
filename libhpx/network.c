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
#include "parcel.h"
#include "scheduler.h"

static network_t *_network = NULL;

int
network_init(void) {
  _network = smp_new();
  return HPX_SUCCESS;
}

int
network_init_thread(void) {
  return HPX_SUCCESS;
}

void
network_fini(void) {
  smp_delete(_network);
}

void
network_fini_thread(void) {
}

// static void *_send_offset(hpx_parcel_t *parcel) {
//   return &parcel->size;
// }

// static int _send_size(hpx_parcel_t *parcel) {
//   int size = parcel->size;
//   size += sizeof(parcel->size);
//   size += sizeof(parcel->action);
//   size += sizeof(parcel->target);
//   size += sizeof(parcel->cont);
//   return size;
// }

void
hpx_parcel_send(hpx_parcel_t *p) {
  scheduler_spawn(p);
  // int rank = hpx_addr_to_rank(p->target);
  // _network->send(rank, _send_offset(p), _send_size(p), NULL);
  // parcel_release(p);
}

void
hpx_parcel_send_sync(hpx_parcel_t *p) {
}

void
network_berrier(void) {
}

bool
network_addr_is_local(hpx_addr_t addr, void **out) {
  *out = (void*)addr;
  return true;
}
