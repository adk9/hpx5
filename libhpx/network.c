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
/// shielding it from the details of the underlying transport interface.
/// ----------------------------------------------------------------------------
#include "network.h"
#include "transport.h"
#include "parcel.h"
#include "scheduler.h"
#include "debug.h"

static transport_t *_transport = NULL;

int
network_init(void) {
  _transport = smp_new();
  return HPX_SUCCESS;
}

int
network_init_thread(void) {
  return HPX_SUCCESS;
}

void
network_fini(void) {
  smp_delete(_transport);
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
  if (out)
    *out = (void*)addr;
  return true;
}
