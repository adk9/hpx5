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
/// @file network/smp.c
///
/// The smp network is used by default when no other network is defined.
/// ----------------------------------------------------------------------------
#include <stdlib.h>
#include "scheduler.h"
#include "networks.h"
#include "parcel.h"

static int _init(void) {
  return HPX_SUCCESS;
}

static void _fini(void) {
}

static int _send(int dest, void *buffer, unsigned size,
                  network_request_t *request) {
  return 0;
}

network_t *
smp_new(void) {
  network_t *smp = malloc(sizeof(*smp));
  smp->init = _init;
  smp->fini = _fini;
  smp->progress = NULL;
  smp->probe = NULL;
  smp->send = _send;
  smp->recv = NULL;
  smp->test_sendrecv = NULL;
  smp->test_send = NULL;
  smp->test_recv = NULL;
  smp->put = NULL;
  smp->get = NULL;
  smp->test_putget = NULL;
  smp->test_put = NULL;
  smp->test_get = NULL;
  smp->pin = NULL;
  smp->unpin = NULL;
  smp->phys_addr = NULL;
  smp->get_network_bytes = NULL;
  smp->barrier = NULL;
  return smp;
}

void
smp_delete(network_t *smp) {
  free(smp);
}
