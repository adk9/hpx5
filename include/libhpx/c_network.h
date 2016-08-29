// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

/// @file include/libhpx/c_network.h
/// @brief Declare the network_t interface.
///
/// This file declares the interface to the parcel network subsystem in HPX. The
/// network's primary responsibility is to accept send requests from the
/// scheduler, and send them out via the configured transport.
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/string.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Forward declarations.
/// @{
struct boot;
struct config;
struct gas;
struct transport;
/// @}

typedef void network_t;

/// Perform one network progress operation.
///
/// This is not synchronized at this point, and must be synchronized
/// externally.
///
/// @param      obj The network to start.
/// @param       id The id to use when progressing the network.
void network_progress(void *obj, int id);

/// Initiate a parcel send over the network.
///
/// This is an asynchronous interface, the parcel will be sent at some point in
/// the future, and is guaranteed to be delivered. Delivery order is not
/// guaranteed. This transfers ownership of the parcel @p p, the client should
/// not access the sent parcel again---in particular, the client should not
/// release the parcel.
///
/// This call will block if there are not enough resources available to satisfy
/// the enqueue. It may progress the network as well, though that behavior is
/// not guaranteed.
///
/// @todo There is currently no way to test for send completion. We should add a
///       future parameter so that the sender can wait if necessary.
///
/// @param          obj The network to use for the send.
/// @param            p The parcel to send.
/// @param        ssync The local synchronization continuation.
///
/// @returns  LIBHPX_OK The send was buffered successfully
int network_send(void *obj, hpx_parcel_t *p, hpx_parcel_t *ssync);

/// Probe for received parcels.
hpx_parcel_t *network_probe(void *obj, int rank);

/// Flush the network to force it to finish its pending operations.
void network_flush(void *obj);

/// Register a memory region for dma access.
///
/// Network registration is a limited resource. Currently, we handle
/// registration failures as unrecoverable. In the future it will make sense to
/// implement a registration cache or other mechanism for resource management.
///
/// @param          obj The network object.
/// @param         base The beginning of the region to register.
/// @param        bytes The number of bytes to register.
/// @param          key The key to use when registering dma.
void network_register_dma(void *obj, const void *base, size_t bytes, void *key);

/// Release a registered memory region.
///
/// The region denotated by @p segment, @p bytes must correspond to a region
/// previously registered.
///
/// @param          obj The network object.
/// @param         base The beginning of the region to release.
/// @param        bytes The number of bytes to release.
void network_release_dma(void *obj, const void *base, size_t bytes);

int network_coll_init(void *obj, void **collective);
int network_coll_sync(void *obj, void *in, size_t in_size, void* out, void *collective);

/// Perform an LCO get operation through the network.
int network_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out, int reset);

/// Perform an LCO wait operation through the network.
int network_lco_wait(void *obj, hpx_addr_t lco, int reset);

int network_memget(void *obj, void *to, hpx_addr_t from, size_t size,
                   hpx_addr_t lsync, hpx_addr_t rsync);

int network_memget_rsync(void *obj, void *to, hpx_addr_t from, size_t size,
                         hpx_addr_t lsync);

int network_memget_lsync(void *obj, void *to, hpx_addr_t from, size_t size);

int network_memput(void *obj, hpx_addr_t to, const void *from, size_t size,
                   hpx_addr_t lsync, hpx_addr_t rsync);

int network_memput_lsync(void *obj, hpx_addr_t to, const void *from,
                         size_t size, hpx_addr_t rsync);

int network_memput_rsync(void *obj, hpx_addr_t to, const void *from,
                         size_t size);

int network_memcpy(void *obj, hpx_addr_t to, hpx_addr_t from, size_t size,
                   hpx_addr_t sync);

int network_memcpy_sync(void *obj, hpx_addr_t to, hpx_addr_t from, size_t size);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_NETWORK_H
