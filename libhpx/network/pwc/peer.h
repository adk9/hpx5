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
#ifndef LIBHPX_NETWORK_PWC_PEER_H
#define LIBHPX_NETWORK_PWC_PEER_H

#include "commands.h"
#include "eager_buffer.h"
#include "segment.h"
#include "send_buffer.h"

struct gas;
struct pwc_xport;

/// Each peer maintains these RDMA segments for remote access.
typedef enum {
  SEGMENT_NULL,
  SEGMENT_HEAP,
  SEGMENT_EAGER,
  SEGMENT_PEERS,
  SEGMENT_MAX
} segid_t;

/// This structure tracks the peer-to-peer information needed for the network.
///
/// Each peer contains three different remote rdma segments, one for the HPX
/// global heap, one for the set of eager parcel recv buffers and one that maps
/// the peer structures themselves.
///
/// The underlying network is expected to support PWC as a native operation, but
/// the peer structure provides buffering to deal with send overflow when the
/// peer isn't processing PWC command events quickly enough. All PWC
/// operations go through the pwc buffer structure and target one of the three
/// remote segments.
///
/// Each peer structure has a local eager buffer where it receives parcels. This
/// buffer space is actually allocated as parcel of the local SEGMENT_PARCEL
/// segment, and is read locally as a result of specific network_probe()
/// operations at the destination.
///
/// Each source has a reference to the remote peer's receive buffer, which it
/// accesses through its local send buffer.
typedef struct peer {
  uint32_t           rank;                      // the peer's rank
  const uint32_t   UNUSED;                      // padding
  segment_t      segments[SEGMENT_MAX];         // RDMA segments
  struct pwc_xport *xport;
  eager_buffer_t       rx;                      // the eager tx endpoint
  eager_buffer_t       tx;                      // the eager rx endpoint
  send_buffer_t      send;                      // the parcel send buffer
} peer_t;

/// Finalize a peer structure.
void peer_fini(peer_t *peer)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Perform a put-with-command operation to a specific peer.
///
/// This simply translates the segment id into the appropriate segment structure
/// for this peer, and then forwards the request through the PWC buffer.
///
/// @param         peer The peer structure representing the target of the pwc.
/// @param         roff The remote offset for the put operation.
/// @param          lva The source buffer for the put.
/// @param            n The number of bytes in the put operation.
/// @param        lsync A local continuation to run.
/// @param        rsync A remote continuation to run.
/// @param   segment_id The segment corresponding to @p roff.
///
/// @return  LIBHPX_OK The operation was successful.
int peer_pwc(peer_t *peer, size_t roff, const void *lva, size_t n,
             command_t lsync, command_t rsync, segid_t segid)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Simply put a command.
int peer_put_command(peer_t *p, command_t rsync)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Perform a parcel send operation to a specific peer.
///
/// This simply forwards the operation through the send buffer structure, which
/// will either put the parcel directly into the remote eager buffer for this
/// peer, or will buffer the parcel to be sent in the future.
///
/// This may be called concurrently by a number of different threads, and
/// expects the send buffers to be properly synchronized.
///
/// @param         peer The peer structure representing the send target.
/// @param            p The parcel to send.
/// @param        lsync An event identifier representing local command.
///
/// @returns  LIBHPX_OK The send operation was initiated successfully..
int peer_send(peer_t *peer, hpx_parcel_t *p, hpx_addr_t lsync)
  HPX_INTERNAL HPX_NON_NULL(1,2);

/// Perform a lazy parcel send operation.
int peer_send_rendezvous(peer_t *peer, hpx_parcel_t *p, hpx_addr_t lsync)
  HPX_INTERNAL HPX_NON_NULL(1,2);

/// Perform a get operation from a specific peer.
///
/// @param         peer The peer structure representing the get target.
/// @param          lva The local virtual address to copy to.
/// @param       offset The remote offset to get from.
/// @param            n The number of bytes to copy.
/// @param         sync An command specification for get command.
/// @param      segment The segment corresponding to @p offset
///
/// @returns  LIBHPX_OK The get operation was initiated successfully.
int peer_get(peer_t *peer, void *lva, size_t offset, size_t n,
             command_t sync, segid_t segment)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Declare an interrupt that can be used to extract the offset from a command,
/// and free it as a parcel.
extern HPX_ACTION_DECL(free_parcel);

#endif
