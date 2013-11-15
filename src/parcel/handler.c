/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Parcel Handler Functions
  hpx_parcelhandler.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Benjamin D. Martin <benjmart [at] indiana.edu>
 ====================================================================
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "parcelhandler.h"                      /*  */
#include "parcelqueue.h"                        /* struct parcelqueue */
#include "predefined_actions.h"                 /* action_set_shutdown_future, and parcel_shutdown_futures*/
#include "request_buffer.h"                     /* struct request_buffer */
#include "request_list.h"                       /* struct request_list */
#include "serialization.h"                      /* struct header */
#include "hashstr.h"                            /* hashstr() */
#include "network.h"
#include "hpx/action.h"
#include "hpx/init.h"
#include "hpx/parcel.h"
#include "hpx/runtime.h"                        /* hpx_get_num_localities() */
#include "hpx/thread/ctx.h"                     /* struct hpx_context */

// #define HPX_PARCELHANDLER_GET_THRESHOLD 0 // force all payloads to be sent via put/get
/* At present, the threshold is used with the payload_size. BUT doesn't it make more sense to compare against total parcel size? The action name factors in then... */
#define HPX_PARCELHANDLER_GET_THRESHOLD SIZE_MAX /* SIZE_MAX ensures all parcels go through send/recv not put/get */

struct parcelqueue *__hpx_send_queue = NULL;

size_t REQUEST_BUFFER_SIZE = 2048; /* TODO: make configurable (and find a good, sane size) */

size_t RECV_BUFFER_SIZE = 1024*1024*16; /* TODO: make configurable (and use a real size) */

hpx_error_t parcel_process(struct header* header) {
  hpx_parcel_t* parcel;
  deserialize(header, &parcel);
  hpx_action_invoke(parcel->action, parcel->payload, NULL);
  return HPX_SUCCESS;
}

typedef int(*test_function_t)(network_request_t*, int*, network_status_t*);

int complete_requests(struct request_list* list,  test_function_t test_func, bool send) {
  network_status_t status;
  network_request_t* req;
  int flag;
  int count = 0;
  struct header* header;
  size_t size;
  int success;

  request_list_begin(list);
  while ((req = request_list_curr(list)) != NULL) {
    success = test_func(req, &flag, &status);
    if (flag == 1 && success == 0) {
      count++;
      header = request_list_curr_parcel(list);
      request_list_del(list);
      if (send) {
	/* Free up the parcel as we don't need it anymore */
	size = get_parcel_size(header);
#if DEBUG_HANDLER
	printf("%d: Unpinning/freeing %zd bytes from buffer at %tx (parcel_id=%d)\n", hpx_get_rank(), size, (ptrdiff_t)header, header->parcel_id);
#endif
	__hpx_network_ops->unpin(header, size);
	hpx_free(header);
      }
      else { /* this is a recv or get */
#if DEBUG_HANDLER
	size = get_parcel_size(header);
	printf("%d: Received %zd bytes to buffer at %tx with parcel_id=%u action=%tu\n", hpx_get_rank(), size, (ptrdiff_t)header, header->parcel_id, (uintptr_t)header->action);
      fflush(stdout);
#endif
	parcel_process(header);
	size = get_parcel_size(header);
#if DEBUG_HANDLER
	printf("%d: Unpinning/freeing %zd bytes from buffer at %tx\n", hpx_get_rank(), size, (ptrdiff_t)header);
#endif
	__hpx_network_ops->unpin(header, size);
	free(header);
      }
    } /* if (flag == 1) */
    request_list_next(list);
  } /* while() */
  return count;
}

void _hpx_parcelhandler_main(void* args) {
  //  printf("Waiting on action registration to complete\n");
  hpx_waitfor_action_registration_complete(); /* make sure action registration is done or else the parcel handler can't invoke actions */

  int success;

  hpx_future_t* quit = ((hpx_parcelhandler_t*)args)->quit;

  struct request_list send_requests;
  struct request_list recv_requests;
  request_list_init(&send_requests);
  request_list_init(&recv_requests);

  struct header* header;
  size_t i;
  int completions;

  network_request_t* req;

  #if DEBUG_HANDLER
    size_t probe_successes, recv_successes, send_successes;
    int initiated_something, completed_something;
  #endif

  int dst_rank; /* raw network address as opposed to internal hpx address */
  size_t size;

  int outstanding_recvs;
  int outstanding_sends;

  void* recv_buffer;
  size_t recv_size;
  int * retval;
  int flag;
  network_status_t status;

  outstanding_recvs = 0;
  outstanding_sends = 0;
  retval = hpx_alloc(sizeof(int));
  *retval = 0;

  i = 0;
#if DEBUG_HANDLER
  probe_successes= 0;
  recv_successes = 0;
  send_successes = 0;
  initiated_something = 0;
  completed_something = 0;
#endif

  while (1) {
    
    /* ==================================
       Phase 1: Deal with sends 
       ==================================
       
       (1) cleanup outstanding sends/puts
       (2) check __hpx_send_queue
       + call network ops to send
    */

    /* cleanup outstanding sends/puts */
    if (outstanding_sends > 0) {
     completions = complete_requests(&send_requests, __hpx_network_ops->send_test, true);
     outstanding_sends -= completions;
#if DEBUG_HANDLER
     if (completions > 0) {
       completed_something = 1;
       send_successes++;
     }
#endif
    } /* if (outstanding_sends > 0) */
    
    /* check send queue */
    header = parcelqueue_trypop(__hpx_send_queue);
    if (header != NULL) {
#if DEBUG_HANDLER
      initiated_something = 1;
#endif
      if (header == NULL) {
	/* TODO: signal error to somewhere else! */
      }
      dst_rank = header->dest.locality.rank;
      size = get_parcel_size(header);
#if DEBUG_HANDLER
      printf("%d: Pinning/allocating %zd bytes at %tx\n", hpx_get_rank(), get_parcel_size(header), (ptrdiff_t)header);
      fflush(stdout);
#endif
      __hpx_network_ops->pin((void*)header, get_parcel_size(header));
#if DEBUG_HANDLER
      printf("%d: Sending %zd bytes from buffer at %tx with parcel_id=%u action=%tu\n", hpx_get_rank(), size, (ptrdiff_t)header, header->parcel_id, (uintptr_t)header->action);
      fflush(stdout);
#endif
      req = request_list_append(&send_requests, header);
#if DEBUG_HANDLER
#if HAVE_PHOTON
      printf("%d: Sending with request at %#tx (%zu) from buffer at %tx\n", hpx_get_rank(), (ptrdiff_t)req, (size_t)req->photon, (ptrdiff_t)header);
#else
      printf("%d: Sending with request at %#tx (%#tx) from buffer at %tx\n", hpx_get_rank(), (ptrdiff_t)req, (ptrdiff_t)req->mpi, (ptrdiff_t)header);
#endif
      fflush(stdout);
#endif
      __hpx_network_ops->send(dst_rank, 
			      header, 
			      size,
			      req);
#if 0
      int temp_flag;
      int temp_ret;
      network_status_t temp_status;
      temp_ret = __hpx_network_ops->sendrecv_test(req, &temp_flag, &temp_status);
      printf("%d: send for req %#tx: temp_ret_ = %d temp_flag = %d\n", hpx_get_rank(), (ptrdiff_t)req, temp_ret, temp_flag);
#endif
      outstanding_sends++;
    }

    /* ==================================
       Phase 2: Deal with remote parcels 
       ==================================
    */
  if (outstanding_recvs > 0) {
    completions = complete_requests(&recv_requests, __hpx_network_ops->recv_test, false);
    outstanding_recvs -= completions;
#if DEBUG_HANDLER
    if (completions > 0) {
      completed_something = 1;
      recv_successes += completions;
    }
#endif      
  }
  
  /* Now check for new receives */
  success = __hpx_network_ops->probe(NETWORK_ANY_SOURCE, &flag, &status);
  if (flag > 0) { /* there is a message to receive */
#if DEBUG_HANDLER
    initiated_something = 1;
    probe_successes++;
#endif
    
#if HAVE_PHOTON
    recv_size = (size_t)status.photon.size;
#else
    recv_size = status.count;
    // recv_size = RECV_BUFFER_SIZE;
#endif
    success = hpx_alloc_align((void**)&recv_buffer, 64, recv_size);
    if (success != 0 || recv_buffer == NULL) {
      __hpx_errno = HPX_ERROR_NOMEM;
      *retval = HPX_ERROR_NOMEM;
      goto error;
    } 
    __hpx_network_ops->pin(recv_buffer, recv_size);
    req = request_list_append(&recv_requests, recv_buffer);
    __hpx_network_ops->recv(status.source, recv_buffer, recv_size, req);
    outstanding_recvs++;
#if 0
    int temp_flag;
    int temp_ret;
    network_status_t temp_status;
    temp_ret = __hpx_network_ops->sendrecv_test(req, &temp_flag, &temp_status);
    printf("%d: recv for req %#tx: temp_ret_ = %d temp_flag = %d\n", hpx_get_rank(), (ptrdiff_t)req, temp_ret, temp_flag);
#endif
#if DEBUG_HANDLER
#if HAVE_PHOTON
    printf("%d: Receiving with request at %#tx (%zu) %zd bytes to buffer at %tx\n", hpx_get_rank(), (ptrdiff_t)req, (size_t)req->photon, recv_size, (ptrdiff_t)recv_buffer);
#else
    printf("%d: Receiving with request at %#tx (%#tx) %zd bytes to buffer at %tx\n", hpx_get_rank(), (ptrdiff_t)req, (ptrdiff_t)req->mpi, recv_size, (ptrdiff_t)recv_buffer);
#endif
    fflush(stdout);
#endif
  }
  
  
#if DEBUG_HANDLER
  if (initiated_something != 0 || completed_something != 0) {
    printf("rank %d: initiated: %d\tcompleted %d\tprobes=%d\trecvs=%d\tsend=%d\n", hpx_get_rank(), initiated_something, completed_something, (int)probe_successes, (int)recv_successes, (int)send_successes);
    initiated_something = 0;
    completed_something = 0;
  }
#endif
  
  if (hpx_lco_future_isset(quit) == true && outstanding_sends == 0 && outstanding_recvs == 0 && parcelqueue_empty(__hpx_send_queue))
    break;
  /* If we don't yield occasionally, any thread that get scheduled to this core will get stuck. */
  i++;
  if (i % 1000 == 0)
    hpx_thread_yield();    
  }

#ifdef DEBUG_HANDLER
  printf("%d: Handler done after iter %d\n", hpx_get_rank(), (int)i);
  fflush(stdout);
#endif

 error:  
  hpx_thread_exit((void*)retval);
}

hpx_parcelhandler_t *hpx_parcelhandler_create(struct hpx_context *ctx) {
  int ret = HPX_ERROR;
  hpx_parcelhandler_t *ph = NULL;
  
  /* create and initialize send queue */
  ret = parcelqueue_create(&__hpx_send_queue);
  if (ret != 0) {
    __hpx_errno = HPX_ERROR;
    goto error;
  }

  /* create thread */
  ph = hpx_alloc(sizeof(*ph));
  ph->ctx = ctx;
  ph->quit = hpx_alloc(sizeof(hpx_future_t));
  hpx_lco_future_init(ph->quit);
  ph->fut = hpx_thread_create(ph->ctx,
                              HPX_THREAD_OPT_SERVICE_COREGLOBAL,
                              _hpx_parcelhandler_main,
                              (void*)ph,
                              &ph->thread);
  /* TODO: error check */

 error:
  return ph;
}

void hpx_parcelhandler_destroy(hpx_parcelhandler_t * ph) {
  unsigned i;
  unsigned num_localities;
  
  /* First, we need to wait for all other localities to reach this point. */
  num_localities = hpx_get_num_localities();

  for (i = 0; i < num_localities; i++) {
    hpx_parcel_t* p = hpx_alloc(sizeof(hpx_parcel_t));
    if (p == NULL) {
      __hpx_errno = HPX_ERROR_NOMEM;
      return;
    }
    struct {size_t rank;} *arg;
    arg = hpx_alloc(sizeof(*arg));
    arg->rank = (size_t)hpx_get_rank();
    hpx_new_parcel(action_set_shutdown_future, arg, sizeof(*arg), p); /* should check error but how would we handle it? */
    hpx_locality_t *loc = hpx_locality_from_rank(i);
    hpx_send_parcel(loc, p); 
    hpx_locality_destroy(loc);
  }
  for (i = 0; i < num_localities; i++)
    hpx_thread_wait(&shutdown_futures[i]);

  /* Now shut down the parcel handler */
  hpx_lco_future_set_state(ph->quit);
  hpx_thread_wait(ph->fut);

  /* Now cleanup any remaining variables */
  hpx_lco_future_destroy(ph->quit);
  hpx_free(ph->quit);
  hpx_free(ph);
  parcelqueue_destroy(&__hpx_send_queue);

  return;
}
