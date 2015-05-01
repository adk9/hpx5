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
#ifndef LIBHPX_NETWORK_H
#define LIBHPX_NETWORK_H

/// @file include/libhpx/network.h
/// @brief Declare the network_t interface.
///
/// This file declares the interface to the parcel network subsystem in HPX. The
/// network's primary responsibility is to accept send requests from the
/// scheduler, and send them out via the configured transport.
#include <hpx/hpx.h>
#include <libhpx/memory.h>

/// Forward declarations.
/// @{
struct boot;
struct config;
struct gas;
struct transport;
/// @}

/// The network interface uses a particular action type, the network *command*,
/// which takes an integer indicating the source of the command, and an optional
/// command argument.

/// Command actions should be declared and defined using the following macros.
/// @{
#define COMMAND_DEF(type, symbol, handler)                              \
    HPX_ACTION(type, 0, symbol, handler, HPX_INT, HPX_UINT64)

#define COMMAND_DECL(symbol) HPX_ACTION_DECL(symbol)
/// @}

/// The release_parcel command will release a parcel.
HPX_INTERNAL extern COMMAND_DECL(release_parcel);

/// The lco_set command will set an lco.
HPX_INTERNAL extern COMMAND_DECL(lco_set);

/// All network objects implement the network interface.
typedef struct network {
  int type;

  void (*delete)(void*);

  int (*progress)(void*);

  int (*send)(void*, hpx_parcel_t *p);

  int (*command)(void*, hpx_addr_t rank, hpx_action_t op, uint64_t args);

  int (*pwc)(void*, hpx_addr_t to, const void *from, size_t n,
             hpx_action_t lop, hpx_addr_t laddr, hpx_action_t rop,
             hpx_addr_t raddr);

  int (*put)(void*, hpx_addr_t to, const void *from, size_t n,
             hpx_action_t lop, hpx_addr_t laddr);

  int (*get)(void*, void *to, hpx_addr_t from, size_t n,
             hpx_action_t lop, hpx_addr_t laddr);

  hpx_parcel_t *(*probe)(void*, int nrx);

  void (*set_flush)(void*);

  memory_register_t register_dma;
  memory_release_t release_dma;
} network_t;

/// Create a new network.
///
/// This depends on the current boot and transport object to be configured in
/// the "here" locality.
///
/// @param          cfg The current configuration.
/// @param         boot The bootstrap network object.
/// @param          gas The global address space.
///
/// @returns            The network object, or NULL if there was an issue.
network_t *network_new(const struct config *cfg, struct boot *boot,
                       struct gas *gas)
  HPX_MALLOC HPX_INTERNAL;

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
  return network->send(network, p);
}


/// Send a network command.
///
/// This sends a remote completion event to a locality. There is no data
/// associated with this command. This is always locally synchronous.
///
/// @param      network The network to use.
/// @param         rank The target rank.
/// @param           op The operation for the command.
/// @param         args The arguments for the command (40 bits packed with op).
static inline int network_command(network_t *network, hpx_addr_t rank,
                                  hpx_action_t op, uint64_t args) {
  return network->command(network, rank, op, args);
}

/// Initiate an rDMA put operation with a remote completion event.
///
/// This will copy @p n bytes between the @p from buffer and the @p to buffer,
/// setting the @p local LCO when the @p from buffer can be reused, and the @p
/// remote LCO when the remote operation is complete.
///
/// Furthermore, it will generate a remote completion event encoding (@p op,
/// @p op_to) at the locality at which @to is currently mapped, allowing
/// two-sided active-message semantics.
///
/// In this context, signaling the @p remote LCO and the delivery of the remote
/// completion via @p op are independent events that potentially proceed in
/// parallel.
///
/// @param      network The network instance to use.
/// @param           to The global target for the put.
/// @param         from The local source for the put.
/// @param            n The number of bytes to put.
/// @param          lop The local continuation operation.
/// @param        lsync The local continuation address.
/// @param          rop The remote continuation operation.
/// @param        op_to The remote continuation address.
///
/// @returns            LIBHPX_OK
static inline int network_pwc(network_t *network,
                              hpx_addr_t to, const void *from, size_t n,
                              hpx_action_t lop, hpx_addr_t lsync,
                              hpx_action_t rop, hpx_addr_t rsync) {
  return network->pwc(network, to, from, n, lop, lsync, rop, rsync);
}

/// Initiate an rDMA put operation with a local completion event.
///
/// This will copy @p n bytes between the @p from buffer and the @p to buffer,
/// setting the @p local LCO when the @p from buffer can be reused, and the @p
/// remote LCO when the remote operation is complete.
///
/// @param      network The network instance to use.
/// @param           to The global target for the put.
/// @param         from The local source for the put.
/// @param            n The number of bytes to put.
/// @param          lop A local continuation, run when @p from can be modified.
/// @param        laddr A local local continuation address.
///
/// @returns            LIBHPX_OK
static inline int network_put(network_t *network,
                              hpx_addr_t to, const void *from, size_t n,
                              hpx_action_t lop, hpx_addr_t laddr) {
  return network->put(network, to, from, n, lop, laddr);
}

/// Initiate an rDMA get operation with a local completion event.
///
/// This will copy @p n bytes between the @p from buffer and the @p to buffer,
/// setting the @p local LCO when the @p from buffer can be accessed.
///
/// @param      network The network instance to use.
/// @param           to The local target for the get.
/// @param         from The global source for the get.
/// @param            n The number of bytes to get.
/// @param          lop A local continuation, run when @p from can be modified.
/// @param        laddr A local local continuation address.
///
/// @returns            LIBHPX_OK
static inline int network_get(network_t *network,
                              void *to, hpx_addr_t from, size_t n,
                              hpx_action_t lop, hpx_addr_t laddr) {
  return network->get(network, to, from, n, lop, laddr);
}

/// Probe for received parcels.
static inline hpx_parcel_t *network_probe(network_t *network, int rank) {
  return network->probe(network, rank);
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

/// Register a memory region for dma access.
///
/// Network registration is a limited resource. Currently, we handle
/// registration failures as unrecoverable. In the future it will make sense to
/// implement a registration cache or other mechanism for resource management.
///
/// @param      network The network object.
/// @param      segment The beginning of the region to register.
/// @param        bytes The number of bytes to register.
static inline void network_register_dma(network_t *network, const void *base,
                                        size_t bytes, void *key) {
  network->register_dma(network, base, bytes, key);
}

/// Release a registered memory region.
///
/// The region denotated by @p segment, @p bytes must correspond to a region
/// previously registered.
///
/// @param      network The network object.
/// @param      segment The beginning of the region to release.
/// @param        bytes The number of bytes to release.
static inline void network_release_dma(network_t *network, const void *base,
                                       size_t bytes) {
  network->release_dma(network, base, bytes);
}

#endif // LIBHPX_NETWORK_H
