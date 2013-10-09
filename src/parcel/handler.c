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

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "parcelhandler.h"                      /*  */
#include "parcelqueue.h"                        /* struct parcelqueue */
#include "request_buffer.h"                     /* struct request_buffer */
#include "request_list.h"                       /* struct request_list */
#include "serialization.h"                      /* struct header */
#include "hashstr.h"                            /* hashstr() */
#include "hpx/action.h"
#include "hpx/init.h"
#include "hpx/network.h"
#include "hpx/parcel.h"
#include "hpx/thread/ctx.h"                     /* struct hpx_context */

static hpx_action_t HPX_PARCELHANDLER_KILL_ACTION = 0;
static hpx_action_t HPX_PARCELHANDLER_GET_ACTION = 0;

// #define HPX_PARCELHANDLER_GET_THRESHOLD 0 // force all payloads to be sent via put/get
/* At present, the threshold is used with the payload_size. BUT doesn't it make more sense to compare against total parcel size? The action name factors in then... */
#define HPX_PARCELHANDLER_GET_THRESHOLD SIZE_MAX

struct parcelqueue *__hpx_send_queue = NULL;
static struct parcelqueue *parcelqueue_local = NULL;
static struct request_buffer network_send_requests;
static struct request_buffer network_recv_requests;
static struct request_list get_requests;
static struct request_list put_requests;

size_t REQUEST_BUFFER_SIZE = 2048; /* TODO: make configurable (and find a good, sane size) */

size_t RECV_BUFFER_SIZE = 1024*1024*32; /* TODO: make configurable (and use a real size) */

hpx_error_t parcel_process(hpx_parcel_t *parcel) {
  hpx_action_invoke(parcel->action, parcel->payload, NULL);
  return HPX_SUCCESS;
}

/*
  For now, I've decided to use just a single parcel handler thread. We
  could split this into two based on (1) network stuff and (2) action
  invocation. BDM
 */
/*
  Original scheme for this (now modified):
  * check if we need to quit
  * check if parcels need sent
  * check if parcels have arrived (do an irecv)
  * if a large parcel notification came in, post a get
  * process one or more received parcels
  * check if outgoing parcels have completed
  * check if puts and gets have completed
 */
void _hpx_parcelhandler_main(void* unused) {
  bool quitting;
  /* int success; LD:unused */
  hpx_parcel_t* parcel;
  /* void* result; LD:unused */ /* for action returns */
  size_t i;

  #if DEBUG
    size_t probe_successes, recv_successes, send_successes;
    int initiated_something, completed_something;
  #endif

  void* parcel_data; /* raw parcel_data from send queue */
  int network_rank; /* raw network address as opposed to internal hpx address */
  size_t network_size; /* total size of raw network data */

  network_request_t recv_request; /* seperate request for this since we need it to persist */
  network_request_t* get_req;
  network_request_t* put_req;
  network_request_t* curr_request;
  bool outstanding_receive;
  int outstanding_gets;
  int outstanding_puts;
  int curr_flag;
  network_status_t curr_status;
  /* int curr_source; LD:unused */
  char* recv_buffer;
  /* size_t recv_size; LD:unused */
  void* get_buffer;
  int * retval;

  quitting = false;
  outstanding_receive = false;
  outstanding_gets = 0;
  outstanding_puts = 0;
  retval = hpx_alloc(sizeof(int));
  *retval = 0;

  recv_buffer = hpx_alloc(RECV_BUFFER_SIZE);
  if (recv_buffer == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    *retval = HPX_ERROR_NOMEM;
  }

#ifdef HAVE_PHOTON
  __hpx_network_ops->pin(recv_buffer, RECV_BUFFER_SIZE);
#endif

  i = 0;
#if DEBUG
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

    //    printf("In phase 1 (sends) on iter %d\n", i);
    //    fflush(stdout);
    /* cleanup outstanding sends/puts */

    if (!request_buffer_empty(&network_send_requests)) {
      curr_request = request_buffer_head(&network_send_requests);
      __hpx_network_ops->test(curr_request, &curr_flag, &curr_status);
      if (curr_flag) {
#if DEBUG
	completed_something = 1;
	send_successes++;
#endif
	request_buffer_pop(&network_send_requests);
      }
    }
    if (outstanding_puts > 0) {
      request_list_begin(&put_requests);
      while ((put_req = request_list_curr(&put_requests)) != NULL) {
    	__hpx_network_ops->test(put_req, &curr_flag, &curr_status);
    	if (curr_flag == 1) {
    	  parcel = request_list_curr_parcel(&put_requests);
    	  outstanding_puts--;
    	  request_list_del(&put_requests);
    	  /* Free up the parcel as we don't need it anymore */
    	  hpx_free(parcel);
    	} /* if (curr_flag == 1) */
    	request_list_next(&put_requests);
      } /* while() */
    } /* if (outstanding_gets > 0) */
    
    /* check send queue */
    if (!request_buffer_full(&network_send_requests)) { /* don't do this if we can't handle any more send requests */
      parcel_data = parcelqueue_trypop(__hpx_send_queue);
      if (parcel_data != NULL) {
#if DEBUG
	initiated_something = 1;
#endif
	/* first sizeof(hpx_parcel_t) bytes of a serialized parcel is
	   actually a hpx_parcel_t: */
	//	parcel = hpx_read_serialized_parcel(parcel_data); 
	parcel = (hpx_parcel_t*)parcel_data;
	if (parcel == NULL) {
	  /* TODO: signal error to somewhere else! */
	}
	network_rank = parcel->dest.locality.rank;
	network_size = sizeof(struct header); /* total size is parcel header size + data: */
	if (parcel->payload_size < HPX_PARCELHANDLER_GET_THRESHOLD) /* if size is under the eager limit, we send the payload also, so we need to add the size */
	  network_size += parcel->payload_size;
	// network_size = sizeof(hpx_parcel_t) + (sizeof(char)*(strlen(parcel->action.name) + 1)) + parcel->payload_size;
	/* total size is parcel header size + action name + data: */
	if(1) { 	/* TODO: check if size is over the eager limit, then use put() instead */
#ifdef HAVE_PHOTON // TODO: make a runtime choice
	  /* need to unpin this again somewhere */
	  __hpx_network_ops->pin(parcel_data, network_size);
#endif
	  __hpx_network_ops->send(network_rank, 
				  parcel_data, 
				  network_size,
				  request_buffer_push(&network_send_requests));
	}
	if (parcel->payload_size >= HPX_PARCELHANDLER_GET_THRESHOLD) { /* if size is over the eager limit, ALSO do a put */
	  put_req = request_list_append(&put_requests, parcel);
	  __hpx_network_ops->put(network_rank, parcel->payload, parcel->payload_size, put_req); 
	}
      } // end if (parcel_data != NULL)
    } // end if (requests_queue_full(network_send_requests)
    
    /* =================================
       Phase 2: Deal with local parcels 
       =================================
       
       Local parcels are generally invoking actions, in which case we
       just invoke said action. 
       
       We also listen for a
       shutdown request from the main thread.
    */
    /* TODO: handle local parcels other than actions - if applicable? */
    //    printf("In phase 2 (actions) on iter %d\n", i);
    //    fflush(stdout);
    
    parcel = (hpx_parcel_t*)parcelqueue_trypop(parcelqueue_local);
    if (parcel != NULL) {
#if DEBUG
      initiated_something = 1;
#endif
      if (parcel->action == HPX_PARCELHANDLER_KILL_ACTION) {
	quitting = true;
	// break;
      }
      else {
        parcel_process(parcel);
      }
    } /* end if (parcel != NULL) */
    
    /* ==================================
       Phase 3: Deal with remote parcels 
       ==================================
    */
    /*printf("In phase 3 (recvs) on iter %d\n", i); 
      fflush(stdout);*/
    if (outstanding_gets > 0) {
      request_list_begin(&get_requests);
      while ((get_req = request_list_curr(&get_requests)) != NULL) {
	__hpx_network_ops->test(get_req, &curr_flag, &curr_status);
	if (curr_flag == 1) {
	  parcel = request_list_curr_parcel(&get_requests);
	  outstanding_gets--;
	  request_list_del(&get_requests);
	  /* Now invoke the action - or whatever */
      parcel_process(parcel);
	} /* if (curr_flag == 1) */
	request_list_next(&get_requests);
      } /* while() */
    } /* if (outstanding_gets > 0) */
    
    if (outstanding_receive) { /* if we're currently waiting on a message */
      
      /* if we've finished receiving the message, move on... else keep waiting */
      __hpx_network_ops->test(&recv_request, &curr_flag, &curr_status);
      
      if (curr_flag) {
#if DEBUG
	completed_something = 1;
	recv_successes++;
#endif
	outstanding_receive = false;
	/* If we've received something, do stuff:
	   * If it's a notificiation of a put, call get() TODO: do this
	   * If it's an action invocation, do that.
	   */
	
	/* TODO: move this to a seperate thread? */
	deserialize((struct header*)recv_buffer, &parcel);
	/* Right now, parcel_deserialize() calls hpx_alloc(). In the
	   future, if that changes, we might have to allocate a new buffer
	   here.... */
	
	if (parcel->payload_size > HPX_PARCELHANDLER_GET_THRESHOLD) { /* do a get */
	  get_buffer = hpx_alloc(parcel->payload_size);
	  if (get_buffer == NULL) {
	    __hpx_errno = HPX_ERROR_NOMEM;
	    *retval = HPX_ERROR_NOMEM;
	  } 
	  parcel->payload = get_buffer; /* make sure parcel's payload actually points to the payload */
	  get_req = request_list_append(&get_requests, parcel);
	  __hpx_network_ops->get(curr_status.source, get_buffer, parcel->payload_size, get_req); 
	}
	else 
      parcel_process(parcel);
	
      } /* end if(curr_flag) */
    }
    else { /* otherwise, see if there's a message to wait for */
      __hpx_network_ops->probe(NETWORK_ANY_SOURCE, &curr_flag, &curr_status);
      if (curr_flag > 0) { /* there is a message to receive */
	
#if DEBUG
	initiated_something = 1;
	probe_successes++;
#endif
#if 0
#message "This isn't actually used, but I wanted to leave it here because I"
#message "don't know why it exists."
#if HAVE_PHOTON
	recv_size = (size_t)curr_status.photon.size;
#else
	recv_size = RECV_BUFFER_SIZE;
#endif
#endif
	//__hpx_network_ops->recv(curr_status.source, recv_buffer, recv_size, &recv_request);
	__hpx_network_ops->recv(curr_status.source, recv_buffer, curr_status.count, &recv_request);
	outstanding_receive = true;
      }
      
    } /* end if(outstanding_receive) */
    
#if DEBUG
    if (initiated_something != 0 || completed_something != 0) {
      printf("rank %d: initiated: %d\tcompleted %d\tprobes=%d\trecvs=%d\tsend=%d\n", hpx_get_rank(), initiated_something, completed_something, (int)probe_successes, (int)recv_successes, (int)send_successes);
      initiated_something = 0;
      completed_something = 0;
    }
#endif
    
    /* TODO: if no sends in queue, nothing with probe, and quitting == true, break */
    if (quitting && request_buffer_empty(&network_send_requests) && !outstanding_receive)
      break;
    
    i++;
    
    /* If we don't yield occasionally, any thread that get scheduled to this core will get stuck. TODO: takeout after resolving this */
    if (i % 1000 == 0)
      hpx_thread_yield();
    
    /*	  printf("Parcel handler _main spinning i = %zd\n", i);*/
  }
  
#if DEBUG
  printf("Handler done after iter %d\n", (int)i);
  fflush(stdout);
#endif
  
#ifdef HAVE_PHOTON
  __hpx_network_ops->unpin(recv_buffer, RECV_BUFFER_SIZE);
#endif
  
  free(recv_buffer);
  
  hpx_thread_exit((void*)retval);
}

hpx_parcelhandler_t *hpx_parcelhandler_create(struct hpx_context *ctx) {
  int ret = HPX_ERROR;
  /* hpx_config_t * cfg = NULL; LD:unused */
  hpx_parcelhandler_t *ph = NULL;

  /* LD: we just want marker keys here
     BUG: hashstr can collide
  */
  HPX_PARCELHANDLER_KILL_ACTION = hashstr("HPX_PARCELHANDLER_KILL_ACTION");
  HPX_PARCELHANDLER_GET_ACTION = hashstr("HPX_PARCELHANDLER_GET_ACTION");
  
  /* create and initialize queue */
  ret = parcelqueue_create(&parcelqueue_local);
  if (ret != 0) {
    __hpx_errno = HPX_ERROR;
    goto error;
  }

  /* create and initialize send queue */
  ret = parcelqueue_create(&__hpx_send_queue);
  if (ret != 0) {
    __hpx_errno = HPX_ERROR;
    goto error;
  }

  /* create and initialize private network queues */

  ret = request_buffer_init(&network_send_requests, REQUEST_BUFFER_SIZE);
  if (ret != 0) {
    ret = HPX_ERROR_NOMEM;
    __hpx_errno = HPX_ERROR_NOMEM;
    goto error;
  }
  ret = request_buffer_init(&network_recv_requests, REQUEST_BUFFER_SIZE);
  if (ret != 0) {
    ret = HPX_ERROR_NOMEM;
    __hpx_errno = HPX_ERROR_NOMEM;
    goto error;
  }
  request_list_init(&get_requests);

  /* create thread */
  ph = hpx_alloc(sizeof(*ph));
  
  ph->ctx = ctx;
  ph->fut = hpx_thread_create(ph->ctx,
                              HPX_THREAD_OPT_SERVICE_COREGLOBAL,
                              _hpx_parcelhandler_main,
                              0,
                              &ph->thread);
  /* TODO: error check */

 error:
  return ph;
}

void hpx_parcelhandler_destroy(hpx_parcelhandler_t * ph) {
  hpx_network_barrier();
  //#ifdef HAVE_MPI
  //  MPI_Barrier(MPI_COMM_WORLD);
  //#endif

  /* int *child_retval; LD:unused */
  
  hpx_parcel_t* kill_parcel;
  /* could possibly use hpx_send_parcel instead */
  kill_parcel = hpx_alloc(sizeof(*kill_parcel));
  if (kill_parcel == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    goto error;
  }
  kill_parcel->action = HPX_PARCELHANDLER_KILL_ACTION;
  parcelqueue_push(parcelqueue_local, (void*)kill_parcel);

  //hpx_thread_wait(ph->fut);
  hpx_thread_join(ph->thread, NULL);

  /* test code */
  /* TODO: remove */
//  printf("Thread joined\n");
//  printf("Value on return = %d\n", *child_retval);
  /* end test code */

  // hpx_thread_destroy(ph->thread);
  hpx_free(ph);

  request_buffer_destroy(&network_send_requests);
  request_buffer_destroy(&network_recv_requests);

  parcelqueue_destroy(&__hpx_send_queue);
  parcelqueue_destroy(&parcelqueue_local);

 error:
  return;
}
