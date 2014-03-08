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

/// ----------------------------------------------------------------------------
/// @file network.h
///
/// This file defines the interface to the network subsystem in HPX.
/// ----------------------------------------------------------------------------

/// ----------------------------------------------------------------------------
/// Network initialization and finalization.
/// ----------------------------------------------------------------------------
HPX_INTERNAL int network_startup(const hpx_config_t *config);
HPX_INTERNAL void network_shutdown(void);

HPX_INTERNAL void network_release(hpx_parcel_t *parcel) HPX_NON_NULL(1);
HPX_INTERNAL void network_send(hpx_parcel_t *parcel) HPX_NON_NULL(1);
HPX_INTERNAL void network_send_sync(hpx_parcel_t *parcel) HPX_NON_NULL(1);

HPX_INTERNAL void network_barrier(void);


#endif // LIBHPX_NETWORK_H
