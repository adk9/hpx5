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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// @file libhpx/network/network.c
/// @brief Manages the HPX network.
///
/// This file deals with the complexities of the HPX network interface,
/// shielding it from the details of the underlying transport interface.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdlib.h>

#include "contrib/uthash/src/utlist.h"
#include "libsync/queues.h"

#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "libhpx/transport.h"
#include "block.h"
#include "request.h"

/// ----------------------------------------------------------------------------
/// Define some of the global constant addresses.
/// ----------------------------------------------------------------------------
const hpx_addr_t HPX_NULL = { NULL, 0 };
const hpx_addr_t HPX_ANYWHERE = { NULL, -1 };


/// ----------------------------------------------------------------------------
/// The network class data.
/// ----------------------------------------------------------------------------
struct network {
  const boot_t        *boot;                    // rank, n_ranks
  transport_t    *transport;                    // byte-based send/recv

  ms_queue_t          sends;                    // half duplex port for send
  ms_queue_t          recvs;                    // half duplex port for recv

  request_t          *free;                     // request freelist
  request_t *pending_sends;                     // outstanding send requests
  request_t *pending_recvs;                     // outstanding recv requests
};


/// ----------------------------------------------------------------------------
/// Some transports care about pinning data before sending it. The transport
/// isn't supposed to depend on the parcel implementation or interface though,
/// so we check to see if a parcel needs to be pinned here, in the network
/// layer.
///
/// We know that parcels are block allocated, so we pin entire blocks at
/// once. This reduces pin-based churn. Once a parcel block is pinned, we don't
/// ever unpin it, since parcels are reused.
///
/// TODO: It might make sense to unpin blocks during shutdown, for debugging
/// purposes (e.g., valgrind might complain about transport-layer resource
/// allocation if we don't unpin them).
/// ----------------------------------------------------------------------------
static void _pin(transport_t *transport, hpx_parcel_t *parcel) {
  block_t *block = block_from_parcel(parcel);
  if (!block_is_pinned(block)) {
    transport_pin(transport, block, block_get_size(block));
    block_set_pinned(block, true);
  }
}


/// ----------------------------------------------------------------------------
/// Requests associate parcels to transport send and receive operations, and
/// list nodes as well. This constructor creates new requests, bound to the
/// passed parcel.
///
/// We try to abstract the transport request interface so that we're not
/// exposing any transport-specific types in headers. The transport request
/// (i.e., the request identifier that we can probe with later), is abstracted
/// as a byte array.
///
/// This allocator mallocs enough space for the current transport's request
/// size.
/// ----------------------------------------------------------------------------
static HPX_MALLOC request_t *_new_request(network_t *network, hpx_parcel_t *p) {
  request_t *r = network->free;
  if (r) {
    LL_DELETE(network->free, r);
    request_init(r, p);
  }
  else {
    int bytes = transport_request_size(network->transport);
    r = request_new(p, bytes);
  }
  return r;
}


/// ----------------------------------------------------------------------------
/// Frees a previously allocated request.
/// ----------------------------------------------------------------------------
static void _delete_request(network_t *network, request_t *request) {
  LL_PREPEND(network->free, request);
}

/// ----------------------------------------------------------------------------
/// Called during network progress to initiate a send with the transport.
/// ----------------------------------------------------------------------------
static bool _try_start_send(network_t *network) {
  hpx_parcel_t *p = sync_ms_queue_dequeue(&network->sends);
  if (!p)
    return false;
  _pin(network->transport, p);

  request_t *r = _new_request(network, p);
  if (!r) {
    dbg_error("could not allocate a network request.\n");
    goto unwind0;
  }

  int dest = hpx_addr_to_rank(p->target);
  int size = sizeof(*p) + p->size;
  if (transport_send(network->transport, dest, p, size, &r->request)) {
    dbg_error("transport failed send.\n");
    goto unwind1;
  }

  LL_PREPEND(network->pending_sends, r);
  return true;

 unwind1:
  _delete_request(network, r);
 unwind0:
  hpx_parcel_release(p);
  return false;
}


/// ----------------------------------------------------------------------------
/// Called during network progress to initiate a recv with the transport.
/// ----------------------------------------------------------------------------
static bool _try_start_recv(network_t *network) {
  int source = TRANSPORT_ANY_SOURCE;
  int bytes = transport_probe(network->transport, &source);
  if (bytes < 1)
    return false;

  // make sure things happened like we expected
  assert(source >= 0);
  assert(source < boot_n_ranks(network->boot));
  assert(source != TRANSPORT_ANY_SOURCE);

  // allocate a parcel to provide the buffer to receive into
  hpx_parcel_t *p = hpx_parcel_acquire(bytes - sizeof(hpx_parcel_t));
  if (!p) {
    dbg_error("could not acquire a parcel of size %d during receive.\n", bytes);
    return false;
  }
  _pin(network->transport, p);

  // allocate a request to track this transport operation
  request_t *r = _new_request(network, p);
  if (!r) {
    dbg_error("could not allocate a network request.\n");
    goto unwind0;
  }

  // get the buffer inside the parcel that we're supposed to receive to, and
  // perform the receive
  if (transport_recv(network->transport, source, p, bytes, &r->request)) {
    dbg_error("could not receive from transport.\n");
    goto unwind1;
  }

  // remember that this receive is pending so that we can poll it later
  LL_PREPEND(network->pending_recvs, r);
  return true;

 unwind1:
  _delete_request(network, r);
 unwind0:
  hpx_parcel_release(p);
  return false;
}


/// ----------------------------------------------------------------------------
/// Finish a send request.
/// ----------------------------------------------------------------------------
static void _finish_send(network_t *n, request_t *r) {
  hpx_parcel_release(r->parcel);
  _delete_request(n, r);
}


/// ----------------------------------------------------------------------------
/// Finish a receive request.
/// ----------------------------------------------------------------------------
static void _finish_recv(network_t *n, request_t *r) {
  sync_ms_queue_enqueue(&n->recvs, r->parcel);
  _delete_request(n, r);
}


/// ----------------------------------------------------------------------------
/// Recursively test a list of requests.
///
/// Tail recursive so it won't use any stack space. Uses the passed function
/// pointer to finish the request.
/// ----------------------------------------------------------------------------
static int _test(network_t *network, void (*finish)(network_t*, request_t*),
                  request_t **curr, int n) {
  request_t *i = *curr;
  if (i == NULL)
    return n;

  int complete = 0;
  int e = transport_test_sendrecv(network->transport, &i->request, &complete);
  if (e)
    dbg_error("transport test failed.\n");

  if (!complete)
    return _test(network, finish, &i->next, n);

  *curr = i->next;
  finish(network, i);
  return _test(network, finish, curr, n + 1);
}


network_t *network_new(const boot_t *boot, transport_t *transport) {
  network_t *n = malloc(sizeof(*n));
  n->boot          = boot;
  n->transport     = transport;

  sync_ms_queue_init(&n->sends);
  sync_ms_queue_init(&n->recvs);

  n->pending_sends = NULL;
  n->pending_recvs = NULL;
  n->free          = NULL;
  return n;
}


void network_delete(network_t *network) {
  if (!network)
    return;

  hpx_parcel_t *p = NULL;

  while ((p = sync_ms_queue_dequeue(&network->sends)))
    hpx_parcel_release(p);
  sync_ms_queue_fini(&network->sends);

  while ((p = sync_ms_queue_dequeue(&network->recvs)))
    hpx_parcel_release(p);
  sync_ms_queue_fini(&network->recvs);

  request_t *i = NULL, *tmp = NULL;

  LL_FOREACH_SAFE(network->free, i, tmp) {
    LL_DELETE(network->free, i);
    request_delete(i);
  }

  if (network->pending_sends)
    dbg_log("abandoning active send.\n");

  LL_FOREACH_SAFE(network->pending_sends, i, tmp) {
    LL_DELETE(network->pending_sends, i);
    request_delete(i);
  }

  if (network->pending_recvs)
    dbg_log("abandoning active recv.\n");

  LL_FOREACH_SAFE(network->pending_recvs, i, tmp) {
    LL_DELETE(network->pending_recvs, i);
    request_delete(i);
  }

  free(network);
}


void network_send(network_t *network, hpx_parcel_t *p) {
  // check loopback via rank, on loopback push into the recv queue
  hpx_addr_t target = hpx_parcel_get_target(p);
  if (hpx_addr_try_pin(target, NULL))
    sync_ms_queue_enqueue(&network->recvs, p);
  else
    sync_ms_queue_enqueue(&network->sends, p);
}


hpx_parcel_t *network_recv(network_t *network) {
  return sync_ms_queue_dequeue(&network->recvs);
}


void network_progress(network_t *network) {
  int sends = _test(network, _finish_send, &network->pending_sends, 0);
  if (sends)
    dbg_log("finished %d sends.\n", sends);

  bool send = _try_start_send(network);
  if (send)
    dbg_log("started a send.\n");

  int recvs = _test(network, _finish_recv, &network->pending_recvs, 0);
  if (recvs)
    dbg_log("finished %d receives.\n", recvs);

  bool recv = _try_start_recv(network);
  if (recv)
    dbg_log("started a recv.\n");
}


void network_barrier(network_t *network) {
  transport_barrier(network->transport);
}
