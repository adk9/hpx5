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

#include <stdlib.h>
#include <assert.h>

#include "contrib/uthash/src/utlist.h"
#include "libsync/queues.h"
#include "libsync/sync.h"

#include "libhpx/btt.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"
#include "block.h"
#include "progress.h"


request_t *request_init(request_t *request, hpx_parcel_t *p) {
  request->parcel = p;
  return request;
}

request_t *request_new(hpx_parcel_t *p, int request) {
  request_t *r = malloc(sizeof(*r) + request);
  if (!r) {
    dbg_error("could not allocate request.\n");
    return NULL;
  }
  return request_init(r, p);
}

void request_delete(request_t *r) {
  if (!r)
    return;
  free(r);
}


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
static void _pin(transport_class_t *transport, hpx_parcel_t *parcel) {
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
static HPX_MALLOC request_t *_new_request(progress_t *progress, hpx_parcel_t *p) {
  request_t *r = progress->free;
  if (r) {
    LL_DELETE(progress->free, r);
    request_init(r, p);
  }
  else {
    int bytes = transport_request_size(here->transport);
    r = request_new(p, bytes);
  }
  return r;
}


/// ----------------------------------------------------------------------------
/// Frees a previously allocated request.
///
/// This uses a freelist algorithm for request nodes.
///
/// @param network - the network
/// @param request - the request
/// ----------------------------------------------------------------------------
static void _delete_request(progress_t *progress, request_t *request) {
  LL_PREPEND(progress->free, request);
}

/// ----------------------------------------------------------------------------
/// Finish a generic request.
/// ----------------------------------------------------------------------------
static void _finish_request(progress_t *progress, request_t *r) {
  _delete_request(progress, r);
}

/// ----------------------------------------------------------------------------
/// Finish a send request.
///
/// This finishes a send by freeing the request's parcel, and then calling the
/// generic finish handler.
///
/// @param n - the network
/// @param r - the request to finish
/// ----------------------------------------------------------------------------
static void _finish_send(progress_t *progress, request_t *r) {
  hpx_parcel_release(r->parcel);
  _finish_request(progress, r);
}

/// ----------------------------------------------------------------------------
/// Finish a receive request.
///
/// This finishes a receive by pushing the request's parcel into the receive
/// queue, and then calling the generic finish handler.
///
/// @param n - the network
/// @param r - the request to finish
/// ----------------------------------------------------------------------------
static void _finish_recv(progress_t *progress, request_t *r) {
  network_rx_enqueue(r->parcel);
  _finish_request(progress, r);
}

/// ----------------------------------------------------------------------------
/// Called during network progress to initiate a send with the transport.
///
/// Try and pop a network request off of the send queue, allocate a request node
/// for it, and initiate a byte-send with the transport.
///
/// @param network - the network object
/// @returns       - true if we initiated a send
/// ----------------------------------------------------------------------------
static bool _try_start_send(progress_t *progress) {
  hpx_parcel_t *p = network_tx_dequeue();
  if (!p)
    return false;
  _pin(here->transport, p);

  request_t *r = _new_request(progress, p);
  if (!r) {
    dbg_error("could not allocate a network request.\n");
    goto unwind0;
  }

  uint32_t dest = btt_owner(here->btt, p->target);
  int size = sizeof(*p) + p->size;
  if (transport_send(here->transport, dest, p, size, &r->request)) {
    dbg_error("transport failed send.\n");
    goto unwind1;
  }

  LL_PREPEND(progress->pending_sends, r);
  return true;

 unwind1:
  _delete_request(progress, r);
 unwind0:
  hpx_parcel_release(p);
  return false;
}


/// ----------------------------------------------------------------------------
/// Called during network progress to initiate a recv with the transport.
/// ----------------------------------------------------------------------------
static bool _try_start_recv(progress_t *progress) {
  int src = TRANSPORT_ANY_SOURCE;
  int size = transport_probe(here->transport, &src);
  if (size < 1)
    return false;

  // make sure things happened like we expected
  assert(src >= 0);
  assert(src < here->ranks);
  assert(src != TRANSPORT_ANY_SOURCE);

  // allocate a parcel to provide the buffer to receive into
  hpx_parcel_t *p = hpx_parcel_acquire(size - sizeof(hpx_parcel_t));
  if (!p) {
    dbg_error("could not acquire a parcel of size %d during receive.\n", size);
    return false;
  }
  _pin(here->transport, p);

  // allocate a request to track this transport operation
  request_t *r = _new_request(progress, p);
  if (!r) {
    dbg_error("could not allocate a network request.\n");
    goto unwind0;
  }

  // get the buffer inside the parcel that we're supposed to receive to, and
  // perform the receive
  if (transport_recv(here->transport, src, p, size, &r->request)) {
    dbg_error("could not receive from transport.\n");
    goto unwind1;
  }

  // remember that this receive is pending so that we can poll it later
  LL_PREPEND(progress->pending_recvs, r);
  return true;

 unwind1:
  _delete_request(progress, r);
 unwind0:
  hpx_parcel_release(p);
  return false;
}


/// ----------------------------------------------------------------------------
/// Recursively test a list of requests.
///
/// Tail recursive so it won't use any stack space. Uses the passed function
/// pointer to finish the request, so that this can be used for different kinds
/// of requests.
///
/// @param network - the network used for testing
/// @param  finish - a callback to finish the request
/// @param    curr - the current request to test
/// @param       n - the current number of completed requests
/// @returns       - the total number of completed requests
/// ----------------------------------------------------------------------------
static int HPX_NON_NULL(1, 2, 3) _test(progress_t *p,
                                       void (*finish)(progress_t*, request_t*),
                                       request_t **curr, int n)
{
  request_t *i = *curr;

  // base case, return the number of finished requests
  if (i == NULL)
    return n;

  // test this request
  int complete = 0;
  int e = transport_test(here->transport, &i->request, &complete);
  if (e)
    dbg_error("transport test failed.\n");

  // test next request, do not increment n
  if (!complete)
    return _test(p, finish, &i->next, n);

  // remove and finish this request, test new next request, increment n
  *curr = i->next;
  finish(p, i);
  return _test(p, finish, curr, n + 1);
}

void network_progress_flush(progress_t *p) {
  bool send = true;
  while (send)
    send = _try_start_send(p);

  while (p->pending_sends)
    _test(p, _finish_send, &p->pending_sends, 0);
}

void network_progress_poll(progress_t *p) {
  int sends = _test(p, _finish_send, &p->pending_sends, 0);
  if (sends)
    dbg_log("finished %d sends.\n", sends);

  bool send = _try_start_send(p);
  if (send)
    dbg_log("started a send.\n");

  int recvs = _test(p, _finish_recv, &p->pending_recvs, 0);
  if (recvs)
    dbg_log("finished %d receives.\n", recvs);

  bool recv = _try_start_recv(p);
  if (recv)
    dbg_log("started a recv.\n");
}

progress_t *network_progress_new() {
  progress_t *p = malloc(sizeof(*p));
  assert(p);
  p->pending_sends = NULL;
  p->pending_recvs = NULL;
  p->free          = NULL;
  return p;
}

void network_progress_delete(progress_t *p) {
  request_t *i = NULL, *tmp = NULL;

  LL_FOREACH_SAFE(p->free, i, tmp) {
    LL_DELETE(p->free, i);
    request_delete(i);
  }

  if (p->pending_sends)
    dbg_log("abandoning active send.\n");

  LL_FOREACH_SAFE(p->pending_sends, i, tmp) {
    transport_request_cancel(here->transport, i);
    LL_DELETE(p->pending_sends, i);
    request_delete(i);
  }

  if (p->pending_recvs)
    dbg_log("abandoning active recv.\n");

  LL_FOREACH_SAFE(p->pending_recvs, i, tmp) {
    transport_request_cancel(here->transport, i);
    LL_DELETE(p->pending_recvs, i);
    request_delete(i);
  }
}
