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
# include "config.h"
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
#include "libhpx/scheduler.h"
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


 /// Frees a previously allocated request.
 ///
 /// This uses a freelist algorithm for request nodes.
 ///
 /// @param progress - the progress object
 /// @param request  - the request
 static void _delete_request(progress_t *progress, request_t *request) {
   request->next = progress->free;
   progress->free = request->next;
 }


 /// Finish a generic request.
 static void _finish_request(progress_t *progress, request_t *r) {
   _delete_request(progress, r);
 }


 /// Finish a send request.
 ///
 /// This finishes a send by freeing the request's parcel, and then calling the
 /// generic finish handler.
 ///
 /// @param progress - the progress object
 /// @param        r - the request to finish
 static void _finish_send(progress_t *progress, request_t *r) {
   assert(r);
   hpx_parcel_release(r->parcel);
   _finish_request(progress, r);
 }

/// Flush a request.
///
/// This finishes a request during shutdown flushing. We cheat by using the
/// _finish_send handler, which does what we want. This name just helps with
/// documentation.
///
static void _flush(progress_t *progress, request_t *r) {
  _finish_send(progress, r);
}


 /// Finish a receive request.
 ///
 /// This finishes a receive by pushing the request's parcel into the receive
 /// queue, and then calling the generic finish handler.
 ///
 /// @param progress - The progress object.
 /// @param        r - The request to finish.
 static void _finish_recv(progress_t *progress, request_t *r) {
   scheduler_spawn(r->parcel);
   _finish_request(progress, r);
 }


 /// Called during network progress to initiate a send with the transport.
 ///
 /// Try and pop a network request off of the send queue, allocate a request node
 /// for it, and initiate a byte-send with the transport.
 ///
 /// @param      progress - The progress object
 ///
 /// @returns 1 if we initiated a send
 static int _try_start_send(progress_t *progress) {
   uint32_t dest;
   int size;
   hpx_parcel_t *p = network_tx_dequeue(here->network);
   if (!p)
     return 0;

   request_t *r = _new_request(progress, p);
   if (!r) {
     dbg_error("could not allocate a network request.\n");
     goto unwind0;
   }

   dest = btt_owner(here->btt, p->target);
   size = sizeof(*p) + p->size;
   if (transport_send(here->transport, dest, p, size, &r->request)) {
     dbg_error("transport failed send.\n");
     goto unwind1;
   }

   r->next = progress->pending_sends;
   progress->pending_sends = r;
   return 1;

  unwind1:
   _delete_request(progress, r);
  unwind0:
   hpx_parcel_release(p);
   return 0;
 }


 /// Called during network progress to initiate a recv with the transport.
 static int _try_start_recv(progress_t *progress) {
   int src = TRANSPORT_ANY_SOURCE;
   int size = transport_probe(here->transport, &src);
   if (size < 1)
     return 0;

   // make sure things happened like we expected
   assert(src >= 0);
   assert(src < here->ranks);
   assert(src != TRANSPORT_ANY_SOURCE);
   assert(size > 0);

   // allocate a parcel to provide the buffer to receive into
   hpx_parcel_t *p = hpx_parcel_acquire(NULL, size - sizeof(*p));
   if (!p) {
     dbg_error("could not acquire a parcel of size %d during receive.\n", size);
     return 0;
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
   return 1;

  unwind1:
   _delete_request(progress, r);
  unwind0:
   hpx_parcel_release(p);
   return 0;
 }


 /// Test a list of results.
 ///
 /// @param   progress The progress object used for testing.
 /// @param     finish A callback to finish the request.
 /// @param       curr The current request to test.
 /// @param          n The current number of completed requests.
 ///
 /// @returns The total number of completed requests.
 static int _test(progress_t *p, request_t **i,
                  void (*finish)(progress_t*, request_t*)) {
   transport_class_t *t = here->transport;
   int n = 0;
   while (*i != NULL) {
     request_t *j = *i;
     int complete = 0;
     int e = transport_test(t, &j->request, &complete);
     dbg_check(e, "transport test failed");

     if (!complete) {
       i = &j->next;
     }
     else {
       *i = j->next;
       finish(p, j);
       ++n;
     }
   }
   return n;
 }


 /// This call tries to "flush" the transport progress queues. It ensures that
 /// all of the pending sends are finished. This is particularly useful during
 /// shutdown where we need the "shutdown-action" parcels to go out before
 /// shutting down the scheduler.
 void network_progress_flush(progress_t *p) {
   bool send = true;
   while (send)
     send = _try_start_send(p);

   // flush the pending sends
   while (p->pending_sends)
     _test(p, &p->pending_sends, _flush);

   // if we have any pending receives, we wait for those to finish as well
   while (p->pending_recvs)
     _test(p, &p->pending_recvs, _flush);
 }

 void network_progress_poll(progress_t *p) {
   int sends = 0;
   int recvs = 0;
   do {
     int recv = 1;
     while (recv && network_progress_can_recv(p)) {
       recv = _try_start_recv(p);
       p->nprecvs += recv;
       DEBUG_IF (recv) {
	 dbg_log_trans("progress: started a recv.\n");
       }
     }

     sends = _test(p, &p->pending_sends, _finish_send);
     assert(sends <= p->npsends);
     p->npsends -= sends;
     DEBUG_IF (sends) {
       dbg_log_trans("progress: finished %d sends.\n", sends);
     }

     recvs = _test(p, &p->pending_recvs, _finish_recv);
     assert(recvs <= p->nprecvs);
     p->nprecvs -= recvs;
     DEBUG_IF (recvs) {
       dbg_log_trans("progress: finished %d receives.\n", recvs);
     }
   } while (network_progress_drain_sends(p) || network_progress_drain_recvs(p));

   int send = 1;
   while (send && network_progress_can_send(p)) {
     send = _try_start_send(p);
     p->npsends += send;
     DEBUG_IF(send) {
       dbg_log_trans("progress: started a send.\n");
     }
   }
}

progress_t *network_progress_new(transport_class_t *t) {
  progress_t *p = malloc(sizeof(*p));
  assert(p);
  p->psend_limit   = t->get_send_limit(t);
  p->npsends       = 0;
  p->pending_sends = NULL;
  p->precv_limit   = t->get_recv_limit(t);
  p->nprecvs       = 0;
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

#if 0
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
#endif
}
