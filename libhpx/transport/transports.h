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

#include "hpx/attributes.h"
#include "libhpx/transport.h"


/// ----------------------------------------------------------------------------
/// The transport interface is defined as an abstract class.
/// ----------------------------------------------------------------------------
struct transport {
  void (*barrier)(void);
  int (*request_size)(void);
  int (*request_cancel)(void *request);
  int (*adjust_size)(int size);
  const char *(*id)(void)
    HPX_RETURNS_NON_NULL;

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

  int (*test)(transport_t *t, void *request, int *out)
    HPX_NON_NULL(1, 2, 3);

  void (*progress)(transport_t *t, bool flush)
    HPX_NON_NULL(1);
};


HPX_INTERNAL transport_t *transport_new_photon(const struct boot *boot) HPX_NON_NULL(1);
HPX_INTERNAL transport_t *transport_new_mpi(const struct boot *boot) HPX_NON_NULL(1);
HPX_INTERNAL transport_t *transport_new_portals(const struct boot *boot) HPX_NON_NULL(1);
HPX_INTERNAL transport_t *transport_new_smp(const struct boot *boot) HPX_NON_NULL(1);

#endif // LIBHPX_TRANSPORTS_H
