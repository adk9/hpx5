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


/// @file include/libhpx/network.h
/// @brief Declare the network_t interface.
///
/// This file declares the interface to the parcel network subsystem in HPX. The
/// network's primary responsibility is to accept send requests from the
/// scheduler, and send them out via the configured transport.
#include <hpx/hpx.h>


/// Forward declarations.
/// @{
struct boot;
struct config;
struct gas;
/// @}


/// All network objects implement the network interface.
typedef struct network {
  const char *(*id)(void);

  void (*delete)(struct network *)
    HPX_NON_NULL(1);

  int (*progress)(struct network *)
    HPX_NON_NULL(1);

  int (*send)(struct network *, hpx_parcel_t *p, hpx_addr_t local)
    HPX_NON_NULL(1, 2);

  int (*pwc)(struct network *, hpx_addr_t to, void *from, size_t n,
             hpx_addr_t local, hpx_addr_t remote, hpx_action_t op)
    HPX_NON_NULL(1);

  int (*put)(struct network *, hpx_addr_t to, void *from, size_t n,
             hpx_addr_t local, hpx_addr_t remote)
    HPX_NON_NULL(1);

  int (*get)(struct network *, void *to, hpx_addr_t from, size_t n,
             hpx_addr_t local)
    HPX_NON_NULL(1, 2);

  hpx_parcel_t *(*probe)(struct network *, int nrx)
    HPX_NON_NULL(1);

  void (*set_flush)(struct network *)
    HPX_NON_NULL(1);
} network_t;


/// Create a new network.
///
/// This depends on the current boot and transport object to be configured in
/// the "here" locality.
///
/// @param         type The type of the network to instantiate.
/// @param         boot The bootstrap network object.
/// @param          gas The global address space.
/// @param          nrx The number of receive queues.
///
/// @returns            The network object, or NULL if there was an issue.
network_t *network_new(struct config *config, struct boot *boot,
                       struct gas *gas, int nrx)
  HPX_NON_NULL(2, 3) HPX_MALLOC HPX_INTERNAL;


/// Delete a network object.
///
/// This does not synchronize. The caller is required to ensure that no other
/// threads may be operating on the network before making this call.
///
/// @param      network The network to delete.
static inline void network_delete(network_t *network) {
  network->delete(network);
}


/// Perform one network progress operation.
///
/// This is not synchronized at this point, and must be synchronized
/// externally.
///
/// @param      network The network to start.
///
/// @returns  LIBHPX_OK The network was progressed without error.
static inline int network_progress(network_t *network) {
  assert(network);
  return network->progress(network);
}


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
/// @param      network The network to use for the send.
/// @param            p The parcel to send.
///
/// @returns  LIBHPX_OK The send was buffered successfully
static inline int network_send(network_t *network, hpx_parcel_t *p) {
  return network->send(network, p, HPX_NULL);
}


/// Initiate an rDMA put operation with a remote completion event.
///
/// This will copy @p n bytes between the @p from buffer and the @p to buffer,
/// setting the @p local LCO when the @p from buffer can be reused, and the @p
/// remote LCO when the remote operation is complete.
///
/// Furthermore, it will generate a remote completion event encoding (@p op,
/// @to) at the locality at which @to is currently mapped, allowing two-sided
/// active-message semantics.
///
/// In this context, signaling the @p remote LCO and the delivery of the remote
/// completion are independent events that potentially proceed in parallel.
///
/// @param      network The network instance to use.
/// @param           to The global target for the put.
/// @param         from The local source for the put.
/// @param            n The number of bytes to put.
/// @param        local An LCO to signal local completion.
/// @param       remote An LCO to signal remote completion.
/// @param           op The remote completion event.
///
/// @returns            LIBHPX_OK
static inline int network_pwc(network_t *network,
                              hpx_addr_t to, void *from, size_t n,
                              hpx_addr_t local, hpx_addr_t remote,
                              hpx_action_t op) {
  return network->pwc(network, to, from, n, local, remote, op);
}


/// Initiate an rDMA put operation with a remote completion event.
///
/// This will copy @p n bytes between the @p from buffer and the @p to buffer,
/// setting the @p local LCO when the @p from buffer can be reused, and the @p
/// remote LCO when the remote operation is complete.
///
/// @param      network The network instance to use.
/// @param           to The global target for the put.
/// @param         from The local source for the put.
/// @param            n The number of bytes to put.
/// @param        local An LCO to signal local completion.
/// @param       remote An LCO to signal remote completion.
///
/// @returns            LIBHPX_OK
static inline int network_put(network_t *network,
                              hpx_addr_t to, void *from, size_t n,
                              hpx_addr_t local, hpx_addr_t remote) {
  return network->put(network, to, from, n, local, remote);
}


/// Initiate an rDMA put operation with a remote completion event.
///
/// This will copy @p n bytes between the @p from buffer and the @p to buffer,
/// setting the @p local LCO when the @p from buffer can be accessed.
///
/// @param      network The network instance to use.
/// @param           to The local target for the get.
/// @param         from The global source for the get.
/// @param            n The number of bytes to get.
/// @param        local An LCO to signal local completion.
///
/// @returns            LIBHPX_OK
static inline int network_get(network_t *network,
                              void *to, hpx_addr_t from, size_t n,
                              hpx_addr_t local) {
  return network->get(network, to, from, n, local);
}


/// Probe for received parcels.
static inline hpx_parcel_t *network_probe(network_t *network, int nrx) {
  return network->probe(network, nrx);
}


/// Set the network's flush-on-shutdown flag.
///
/// Normally the network progress engine will cancel outstanding requests when
/// it shuts down. This will change that functionality to flush the outstanding
/// requests during shutdown. This is used to ensure that the hpx_shutdown()
/// broadcast operation is sent successfully before the local network stops
/// progressing.
///
/// @param      network The network to modify.
static inline void network_flush_on_shutdown(network_t *network) {
  network->set_flush(network);
}


#endif // LIBHPX_NETWORK_H
