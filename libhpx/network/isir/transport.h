// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef LIBHPX_NETWORK_ISIR_TRANSPORT_H
#define LIBHPX_NETWORK_ISIR_TRANSPORT_H

#include <libhpx/config.h>

struct boot;

hpx_transport_t isir_transport_type(void *transport)
  HPX_INTERNAL;

void *isir_transport_new_mpi(const config_t *cfg)
  HPX_INTERNAL;

void *isir_transport_new(const config_t *cfg, struct boot *boot)
  HPX_INTERNAL;


#endif // LIBHPX_NETWORK_ISIR_TRANSPORT_H
