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
/// @file smp/transport.c
///
/// The smp transport is used by default when no other network is defined.
/// ----------------------------------------------------------------------------
#include <stdlib.h>
#include <hpx.h>
#include "libhpx/debug.h"
#include "libhpx/transport.h"
#include "transports.h"


static void _barrier(void) {
}


static int _request_size(void) {
  return 0;
}


static int _adjust_size(int size) {
  return size;
}


static void _delete(transport_t *transport) {
  free(transport);
}


static void _pin(transport_t *transport, const void* buffer, size_t len) {
}


static void _unpin(transport_t *transpor, const void* buffer, size_t len) {
}


static int _send(transport_t *t, int d, const void *b, size_t n, void *r) {
  dbg_error("should never call send in smp network.\n");
  return HPX_ERROR;
}


static size_t _probe(transport_t *transport, int *src) {
  return 0;
}


static int _recv(transport_t *t, int src, void *buffer, size_t size, void *r) {
  dbg_error("should never receive a parcel in smp network.\n");
  return HPX_ERROR;
}


static int _test(transport_t *t, const void *request, int *success) {
  dbg_error("should never call test in smp network.\n");
  return HPX_ERROR;
}

transport_t *transport_new_smp(const struct boot *boot) {
  transport_t *smp = malloc(sizeof(*smp));

  smp->barrier      = _barrier;
  smp->request_size = _request_size;
  smp->adjust_size  = _adjust_size;

  smp->delete       = _delete;
  smp->pin          = _pin;
  smp->unpin        = _unpin;
  smp->send         = _send;
  smp->probe        = _probe;
  smp->recv         = _recv;
  smp->test         = _test;
  return smp;
}
