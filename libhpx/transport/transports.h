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
#ifndef LIBHPX_TRANSPORTS_H
#define LIBHPX_TRANSPORTS_H

#include "attributes.h"
#include "libhpx/transport.h"


/// ----------------------------------------------------------------------------
/// The transport interface is defined as an abstract class.
/// ----------------------------------------------------------------------------
struct transport {
  /* static */ void (*barrier)(void);
  /* static */ int (*request_size)(void);
  /* static */ int (*adjust_size)(int size);

  void (*delete)(transport_t*)
    HPX_NON_NULL(1);

  void (*pin)(transport_t*, const void* buffer, size_t len)
    HPX_NON_NULL(1);

  void (*unpin)(transport_t*, const void* buffer, size_t len)
    HPX_NON_NULL(1);

  int (*send)(transport_t*, int dest, const void *buffer, size_t size,
              void *request)
    HPX_NON_NULL(1, 3);

  size_t (*probe)(transport_t *, int *src)
    HPX_NON_NULL(1, 2);

  int (*recv)(transport_t *t, int src, void *buffer, size_t size, void *request)
    HPX_NON_NULL(1, 3);

  int (*test)(transport_t *t, const void *request, int *out)
    HPX_NON_NULL(1, 2, 3);


  // int (*init)(void);
  // void (*fini)(void);
  // void (*progress)(void *data);
  // int (*probe)(int src, int *flag, transport_status_t *status);
  // int (*recv)(int src, void *buffer, unsigned size, transport_request_t *request);
  // int (*test_sendrecv)(transport_request_t *request, int *flag, transport_status_t *status);
  // int (*test_send)(transport_request_t *request, int *flag, transport_status_t *status);
  // int (*test_recv)(transport_request_t *request, int *flag, transport_status_t *status);
  // int (*put)(int dest, void *buffer, unsigned len, transport_request_t *request);
  // int (*get)(int dest, void *buffer, unsigned len, transport_request_t *request);
  // int (*test_putget)(transport_request_t *request, int *flag, transport_status_t *status);
  // int (*test_put)(transport_request_t *request, int *flag, transport_status_t *status);
  // int (*test_get)(transport_request_t *request, int *flag, transport_status_t *status);

  // int (*phys_addr)(int *locality);
  // unsigned (*get_transport_bytes)(unsigned n);
};


HPX_INTERNAL transport_t *transport_new_photon(const struct boot *boot) HPX_NON_NULL(1);
HPX_INTERNAL transport_t *transport_new_mpi(const struct boot *boot) HPX_NON_NULL(1);
HPX_INTERNAL transport_t *transport_new_smp(const struct boot *boot) HPX_NON_NULL(1);

#endif // LIBHPX_TRANSPORTS_H
