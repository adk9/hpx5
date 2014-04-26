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
#include "hpx/hpx.h"
#include "libhpx/debug.h"
#include "libhpx/transport.h"

static const char *_id(void) {
  return "SMP";
}

static void _barrier(void) {
}


static int _request_size(void) {
  return 0;
}


static int _request_cancel(void *request) {
  return 0;
}


static int _adjust_size(int size) {
  return size;
}


static void _delete(transport_class_t *transport) {
}


static void _pin(transport_class_t *transport, const void* buffer, size_t len) {
}


static void _unpin(transport_class_t *transpor, const void* buffer, size_t len) {
}


static int _send(transport_class_t *t, int d, const void *b, size_t n, void *r) {
  dbg_error("should never call send in smp network.\n");
  return HPX_ERROR;
}


static size_t _probe(transport_class_t *transport, int *src) {
  return 0;
}


static int _recv(transport_class_t *t, int src, void *buffer, size_t size, void *r) {
  dbg_error("should never receive a parcel in smp network.\n");
  return HPX_ERROR;
}


static int _test(transport_class_t *t, void *request, int *success) {
  dbg_error("should never call test in smp network.\n");
  return HPX_ERROR;
}


static void _progress(transport_class_t *transport, bool flush) {
}

static transport_class_t _smp = {
  .type           = HPX_TRANSPORT_SMP,
  .id             = _id,
  .barrier        = _barrier,
  .request_size   = _request_size,
  .request_cancel = _request_cancel,
  .adjust_size    = _adjust_size,
  .delete         = _delete,
  .pin            = _pin,
  .unpin          = _unpin,
  .send           = _send,
  .probe          = _probe,
  .recv           = _recv,
  .test           = _test,
  .testsome       = NULL,
  .progress       = _progress,
};


transport_class_t *transport_new_smp(void) {
  return &_smp;
}
