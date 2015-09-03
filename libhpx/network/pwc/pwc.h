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

#ifndef LIBHPX_NETWORK_PWC_PWC_H
#define LIBHPX_NETWORK_PWC_PWC_H

#include <hpx/hpx.h>
#include <libhpx/network.h>

/// Forward declarations.
/// @{
struct boot;
struct config;
struct gas;
struct parcel_emulator;
struct pwc_xport;
struct send_buffer;
/// @}

typedef struct {
  network_t                   vtable;
  const struct config           *cfg;
  struct pwc_xport            *xport;
  struct parcel_emulator    *parcels;
  struct send_buffer   *send_buffers;
  struct heap_segment *heap_segments;
} pwc_network_t;

/// Allocate and initialize a PWC network instance.
network_t *network_pwc_funneled_new(const struct config *cfg, struct boot *boot,
                                    struct gas *gas)
  HPX_MALLOC;

/// Perform a PWC network command.
///
/// This sends a "pure" command to the scheduler at a different rank, without
/// any additional argument data. This can avoid using any additional eager
/// parcel buffer space, and can always be satisfied with one low-level "put"
/// operation.
///
/// @param      network The network object pointer.
/// @param          loc The GAS global address of the target rank.
/// @param          rop The network command operation.
/// @param         args The network command argument.
///
/// @returns            The (local) status of the put operation.
int pwc_command(void *network, hpx_addr_t loc, hpx_action_t rop, uint64_t args);

/// Perform an LCO wait operation through the PWC network.
///
/// This performs a wait operation on a remote LCO. It allows the calling thread
/// to wait without an intermediate "proxy" future allocation.
///
/// @param      network The network object pointer.
/// @param          lco The global address of the LCO to wait for.
/// @param        reset True if the wait should also reset the LCO.
///
/// @returns            The status set in the LCO.
int pwc_lco_wait(void *network, hpx_addr_t lco, int reset);

/// Perform an LCO get operation through the PWC network.
///
/// This performs an LCO get operation on a remote LCO. It allows the calling
/// thread to get an LCO value without having to allocate an intermediate
/// "proxy" future and its associated redundant copies.
///
/// @param      network The network object pointer.
/// @param          lco The global address of the LCO to wait for.
/// @param            n The size of the output buffer, in bytes.
/// @param          out The output buffer---must be from registered memory.
/// @param        reset True if the wait should also reset the LCO.
///
/// @returns            The status set in the LCO.
int pwc_lco_get(void *network, hpx_addr_t lco, size_t n, void *out, int reset);

/// Perform a rendezvous parcel send operation.
///
/// For normal size parcels, we use the set of one-to-one pre-allocated eager
/// parcel buffers to put the parcel data directly to the target rank. For
/// larger parcels that will either always overflow the eager buffer, or that
/// will use them up quickly and cause lots of re-allocation synchronization, we
/// use this rendezvous protocol.
///
/// @param      network The network object pointer.
/// @param            p The parcel to send.
///
/// @returns            The status of the operation.
int pwc_rendezvous_send(void *network, const hpx_parcel_t *p);

#endif
