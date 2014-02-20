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
/// @{
HPX_INTERNAL int network_init(void);
HPX_INTERNAL int network_init_thread(void);
HPX_INTERNAL void network_fini(void);
HPX_INTERNAL void network_fini_thread(void);
/// @}

/// ----------------------------------------------------------------------------
/// Wrap a network barrier.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void network_barrier(void);


// The global addressing mechanism is the network's responsibility.

/// ----------------------------------------------------------------------------
/// Get the local address for a global address.
///
/// HPX_NULL will always return true and set @p out to NULL. A remote address
/// will return false, but possibly leaves the out parameter in an undefined
/// state.
///
/// @param     addr - the global address to check
/// @param[out] out - the local address (possibly NULL)
/// @returns        - true if the address is local, false if it is not
/// ----------------------------------------------------------------------------
HPX_INTERNAL bool network_addr_is_local(hpx_addr_t addr, void **out);

#endif // LIBHPX_NETWORK_H
