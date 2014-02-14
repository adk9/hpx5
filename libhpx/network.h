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

#ifndef LIBHPX_NETWORK_H
#define LIBHPX_NETWORK_H

#include "hpx.h"

HPX_INTERNAL void libhpx_network_send(hpx_parcel_t *p);
HPX_INTERNAL void libhpx_network_send_sync(hpx_parcel_t *p);

#endif // LIBHPX_NETWORK_H
