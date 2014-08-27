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

#include "libsync/queues.h"
#include "libsync/sync.h"

#include "libhpx/btt.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"
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
    progress->free = progress->free->next;
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
  request->next = progress->free;
  progress->free = request->next;
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
  network_rx_enqueue(here->network, r->parcel);
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
  hpx_parcel_t *p = network_tx_dequeue(here->network);
  if (!p)
    return false;

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

  r->next = progress->pending_sends;
  progress->pending_sends = r;
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
  assert(size > 0);

  // allocate a parcel to provide the buffer to receive into
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, size - sizeof(*p));
  if (!p) {
    dbg_error("could not acquire a parcel of size %d during receive.\n", size);
    return false;
  }

  // remember the source
  p->src = src;

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
  r->next = progress->pending_recvs;
  progress->pending_recvs = r;
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


/// ----------------------------------------------------------------------------
/// This call tries to "flush" the transport progress queues. It
/// ensures that all of the pending sends are finished. This is
/// particularly useful during shutdown where we need the
/// "shutdown-action" parcels to go out before shutting down the
/// scheduler.
/// ----------------------------------------------------------------------------
void network_progress_flush(progress_t *p) {
  bool send = true;
  while (send)
    send = _try_start_send(p);

  // flush the pending sends
  while (p->pending_sends)
    _test(p, _finish_send, &p->pending_sends, 0);

  // if we have any pending receives, we wait for those to finish as well
  while (p->pending_recvs)
    _test(p, _finish_recv, &p->pending_recvs, 0);
}

void network_progress_poll(progress_t *p) {
  int sends = _test(p, _finish_send, &p->pending_sends, 0);
  if (sends)
    dbg_log_trans("progress: finished %d sends.\n", sends);

  bool send = _try_start_send(p);
  if (send)
    dbg_log_trans("progress: started a send.\n");

  int recvs = _test(p, _finish_recv, &p->pending_recvs, 0);
  if (recvs)
    dbg_log_trans("progress: finished %d receives.\n", recvs);

  bool recv = _try_start_recv(p);
  if (recv)
    dbg_log_trans("progress: started a recv.\n");
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
  request_t *i = NULL;

  while ((i = p->free) != NULL) {
    p->free = p->free->next;
    request_delete(i);
  }

  if (p->pending_sends)
    dbg_log_trans("progress: abandoning active send.\n");

  while ((i = p->pending_sends) != NULL) {
    transport_request_cancel(here->transport, i);
    p->pending_sends = p->pending_sends->next;
    request_delete(i);
  }

  if (p->pending_recvs)
    dbg_log_trans("progress: abandoning active recv.\n");

  while ((i = p->pending_recvs) != NULL) {
    transport_request_cancel(here->transport, i);
    p->pending_recvs = p->pending_recvs->next;
    request_delete(i);
  }
}
