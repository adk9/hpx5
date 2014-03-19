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
/// @file libhpx/network/transport/transport.c
/// @brief Handles transport initialization.
/// ----------------------------------------------------------------------------
#include "libhpx/debug.h"
#include "libhpx/transport.h"
#include "transports.h"

transport_t *transport_new(const struct boot *boot) {
  transport_t *transport = NULL;

#ifdef HAVE_PHOTON
  transport = transport_new_photon(boot);
  if (transport) {
    dbg_log("initialized the Photon transport.\n");
    return transport;
  }
#endif

#ifdef HAVE_MPI
  transport = transport_new_mpi(boot);
  if (transport) {
    dbg_log("initialized the MPI transport.\n");
    return transport;
  }
#endif

  transport = transport_new_smp(boot);
  if (transport) {
    dbg_log("initialized the SMP transport.\n");
    return transport;
  }

  dbg_error("failed to initialize a transport.\n");
  return NULL;
}

const char *transport_id(transport_t *transport) {
  return transport->id();
}

void transport_delete(transport_t *transport) {
  transport->delete(transport);
}


int transport_request_size(const transport_t *transport) {
  return transport->request_size();
}


void transport_pin(transport_t *transport, const void *buffer, size_t len) {
  transport->pin(transport, buffer, len);
}


void transport_unpin(transport_t *transport, const void *buffer, size_t len) {
  transport->pin(transport, buffer, len);
}


int transport_send(transport_t *transport, int dest, const void *buffer,
                   size_t size, void *request) {
  return transport->send(transport, dest, buffer, size, request);
}


size_t transport_probe(transport_t *transport, int *src) {
  return transport->probe(transport, src);
}


int transport_recv(transport_t *transport, int src, void *buffer, size_t n,
                   void *r) {
  return transport->recv(transport, src, buffer, n, r);
}

int transport_test_sendrecv(transport_t *transport, const void *request, int *out) {
  return transport->test(transport, request, out);
}

int transport_adjust_size(transport_t *transport, int size) {
  return transport->adjust_size(size);
}

void transport_barrier(transport_t *transport) {
  transport->barrier();
}
