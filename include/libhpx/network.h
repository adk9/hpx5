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
HPX_INTERNAL int network_init_module(const hpx_config_t *config);
HPX_INTERNAL void network_fini_module(void);
/// @}

/// ----------------------------------------------------------------------------
/// The hpx_parcel structure is what the user-level interacts with.
///
/// The layout of this structure is both important and subtle. The go out onto
/// the network directly, hopefully without being copied in any way. We make sure
/// that the relevent parts of the parcel (i.e., those that need to be sent) are
/// arranged contiguously.
///
/// NB: if you change the layout here, make sure that the network system can deal
/// with it.
///
/// @field    next - intrusive parcel list pointer
/// @field  thread - thread executing this active message
/// @field    data - the data payload associated with this parcel
/// @field    size - the data size in bytes
/// @field  action - the target action identifier
/// @field  target - the target address for parcel_send()
/// @field    cont - the continuation address
/// @field payload - possible in-place payload
/// ----------------------------------------------------------------------------
struct hpx_parcel {
  // these fields are only valid within a locality
  hpx_parcel_t *next;
  struct thread *thread;
  void *data;

  // fields below are sent on wire
  int size;
  hpx_action_t action;
  hpx_addr_t target;
  hpx_addr_t cont;
  char payload[];
};

HPX_INTERNAL void network_release(hpx_parcel_t *parcel) HPX_NON_NULL(1);

/// ----------------------------------------------------------------------------
/// Wrap a network barrier.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void network_barrier(void);

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

/// ----------------------------------------------------------------------------
/// ----------------------------------------------------------------------------
HPX_INTERNAL hpx_addr_t network_malloc(int size, int alignment);

/// ----------------------------------------------------------------------------
/// ----------------------------------------------------------------------------
HPX_INTERNAL void network_free(hpx_addr_t addr);

#endif // LIBHPX_NETWORK_H
