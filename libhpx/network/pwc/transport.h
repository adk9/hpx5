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
#ifndef LIBHPX_NETWORK_PWC_TRANSPORT_H
#define LIBHPX_NETWORK_PWC_TRANSPORT_H

#include <libhpx/config.h>

struct boot;

hpx_transport_t pwc_transport_type(void *transport)
  HPX_INTERNAL;

void *pwc_transport_new_photon(const config_t *config, struct boot *boot)
  HPX_INTERNAL;

void *pwc_transport_new(const config_t *config, struct boot *boot)
  HPX_INTERNAL;

#endif // LIBHPX_NETWORK_PWC_TRANSPORT_H
