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

#include "hpx.h"
//#include <mpi.h>

#define _HPX_PARCELHANDLER_KILL_ACTION 0xdead

hpx_parcelqueue_t * __hpx_parcelqueue_local = NULL;
hpx_parcelqueue_t * __hpx_send_queue = NULL;

/**
   Private storage for network requests. Request queues are ring
buffers and are not intended to be used concurrently.
  */
typedef struct request_queue_t {
  network_request_t *requests;
  size_t head;
  size_t size;
  size_t limit;
} request_queue_t;

int request_queue_init(request_queue_t* q, size_t limit) {
  int ret;
  ret = HPX_ERROR;
  q->requests = hpx_alloc(sizeof(network_request_t)*limit); 
  if (q->requests == NULL) {
    ret = HPX_ERROR_NOMEM;
    __hpx_errno = HPX_ERROR_NOMEM;
  }
  else
    ret = 0;
  q->head = 0;
  q->size = 0;
  q->limit = limit;
  
  return ret;
}

int request_queue_destroy(request_queue_t* q) {
  hpx_free(q->requests);
  return 0;
}

inline bool request_queue_full(request_queue_t* q) {
  if (q != NULL && q->size < q->limit)
    return false;
  else
    return true;
}

inline bool request_queue_empty(request_queue_t* q) {
  if (q != NULL && q->size > 0)
    return false;
  else
    return true;
}

/* caller must check requeue_queue_full first */
network_request_t* request_queue_push(request_queue_t* q) {
  network_request_t* ret = NULL;
  if (!request_queue_full(q)) {
    ret = &(q->requests[q->head + q->size]);
    q->size++;
  }
  return ret;
}

network_request_t* request_queue_head(request_queue_t* q) {
  return &(q->requests[q->head]);
}

void request_queue_pop(request_queue_t* q) {
  if (!request_queue_empty(q))
    q->head++;
  if (q->head == q->limit) /* wrap around */
    q->head = 0;
  q->size--;
}

int hpx_parcelqueue_create(hpx_parcelqueue_t** q_handle) {
  hpx_parcelqueue_t* q = *q_handle;
  int ret;
  int temp;
  hpx_parcelqueue_node_t* node;
  ret = HPX_ERROR;

  q = hpx_alloc(sizeof(hpx_parcelqueue_t));
  if (q == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    ret = HPX_ERROR_NOMEM;
    goto error;
  }

  node = hpx_alloc(sizeof(hpx_parcelqueue_node_t));
  if (node == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    ret = HPX_ERROR_NOMEM;
    free(q);
    goto error;
  }
  node->next = NULL;
  q->head = node;
  q->tail = node;

  ret = pthread_mutex_init(&(q->lock), NULL);
  if (ret != 0) { /* TODO: better error handling */
    ret = HPX_ERROR;
    __hpx_errno = HPX_ERROR;
    free(node);
    free(q);
    goto error;
  }

  *q_handle = q;

 error:
  return ret;
}

void* hpx_parcelqueue_trypop(hpx_parcelqueue_t* q) {
  void* val;
  hpx_parcelqueue_node_t* node;
  hpx_parcelqueue_node_t* new_head;

  if (q == NULL) {
    __hpx_errno = HPX_ERROR; /* TODO: more specific error */
    val = NULL;
    goto error;
  }

  node = q->head;
  new_head = node->next;
  
  if (new_head != NULL) { /* i.e. is not empty */
    val = new_head->value; /* looks weird but it's correct */
    q->head = new_head;
    hpx_free(node);
  }
  else
    val = NULL; /* queue is empty so indicate that */
 error:
  return val;
}

int hpx_parcelqueue_push(hpx_parcelqueue_t* q, void* val) {
  int ret;
  int temp;
  hpx_parcelqueue_node_t* node;
  ret = HPX_ERROR;
  node = NULL;

  if (q == NULL) {
    ret = HPX_ERROR; /* TODO: more specific error */
    goto error;
  }

  node = hpx_alloc(sizeof(hpx_parcelqueue_node_t));
  if (node == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    ret = HPX_ERROR_NOMEM;
    goto error;
  }
  node->next = NULL;
  node->value = val;

  /* CRITICAL SECTION */
  temp = pthread_mutex_lock(&(q->lock));
  if (temp != 0) /* TODO: real error handling */
    goto error;

  q->tail->next = node;
  q->tail = node;

  temp = pthread_mutex_unlock(&(q->lock));
  if (temp != 0) /* TODO: real error handling */
    goto error;
  /* END CRITICAL SECTION */

  ret = 0;

 error:
  return ret;
}

/* for use with single reader and writer ONLY!!!! */
int hpx_parcelqueue_push_nb(hpx_parcelqueue_t* q, void* val) {
  int ret;
  int temp;
  hpx_parcelqueue_node_t* node;
  ret = HPX_ERROR;
  node = NULL;

  if (q == NULL) {
    ret = HPX_ERROR; /* TODO: more specific error */
    goto error;
  }

  node = hpx_alloc(sizeof(hpx_parcelqueue_node_t));
  if (node == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    ret = HPX_ERROR_NOMEM;
    goto error;
  }
  node->next = NULL;
  node->value = val;

  q->tail->next = node;
  q->tail = node;

  ret = 0;

 error:
  return ret;
}


int hpx_parcelqueue_destroy(hpx_parcelqueue_t** q_handle) {
  hpx_parcelqueue_t* q = *q_handle;
  int ret;
  ret = HPX_ERROR;

  if (q == NULL) {
    __hpx_errno = HPX_ERROR; /* TODO: more specific error */
    ret = HPX_ERROR; /* TODO: more specific error */
    goto error;
  }
  pthread_mutex_destroy(&(q->lock));
  hpx_free(q);

  ret = 0;

  q_handle = NULL;
 error:
  return ret;
}

request_queue_t network_send_requests;
request_queue_t network_recv_requests;
request_queue_t network_put_requests;
request_queue_t network_get_requests;

size_t REQUEST_QUEUE_SIZE = 2048; /* TODO: make configurable (and find a good, sane size) */

size_t RECV_BUFFER_SIZE = 10240; /* TODO: make configurable (and use a real size) */

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
void * _hpx_parcelhandler_main(void) {
  bool quitting;
  int success;
  hpx_parcel_t* parcel;
  void* result; /* for action returns */
  size_t i;

  #if DEBUG
    size_t probe_successes, recv_successes, send_successes;
    int initiated_something, completed_something;
  #endif

  void* parcel_data; /* raw parcel_data from send queue */
  int network_rank; /* raw network address as opposed to internal hpx address */
  size_t network_size; /* total size of raw network data */

  network_request_t recv_request; /* seperate request for this since we need it to persist */
  network_request_t* curr_request;
  bool outstanding_receive;
  int curr_flag;
  network_status_t curr_status;
  int curr_source;
  char* recv_buffer;
  int * retval;

  quitting = false;
  outstanding_receive = false;
  retval = hpx_alloc(sizeof(int));
  *retval = 0;

  recv_buffer = hpx_alloc(RECV_BUFFER_SIZE);
  if (recv_buffer == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    *retval = HPX_ERROR_NOMEM;
  }

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
    if (!request_queue_empty(&network_send_requests)) {
      curr_request = request_queue_head(&network_send_requests);
      __hpx_network_ops->sendrecv_test(curr_request, &curr_flag, &curr_status);
      if (curr_flag) {
	#if DEBUG
	  completed_something = 1;
	  send_successes++;
	#endif
	request_queue_pop(&network_send_requests);
      }
    }

    /* check send queue */
    if (!request_queue_full(&network_send_requests)) { /* don't do this if we can't handle any more send requests */
      parcel_data = hpx_parcelqueue_trypop(__hpx_send_queue);
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
	network_size = sizeof(hpx_parcel_t) + (sizeof(char)*(strlen(parcel->action.name) + 1)) + parcel->payload_size; /* total size is parcel header size + action name + data: */
	if(1) { 	/* TODO: check if size is over the eager limit, then use put() instead */
	  __hpx_network_ops->send(network_rank, 
				  parcel_data, 
				  network_size,
				  request_queue_push(&network_send_requests));
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
    parcel = (hpx_parcel_t*)hpx_parcelqueue_trypop(__hpx_parcelqueue_local);
    if (parcel != NULL) {
      #if DEBUG
        initiated_something = 1;
      #endif
      if (parcel->action.action == (hpx_func_t)_HPX_PARCELHANDLER_KILL_ACTION) {
	quitting = true;
	// break;
      }
      else {
	/* invoke action */
	if (parcel->action.action != NULL) {
	  success = hpx_action_invoke(&(parcel->action), parcel->payload, &result);
	}
      }
    } /* end if (parcel != NULL) */
    
    /* ==================================
       Phase 3: Deal with remote parcels 
       ==================================
    */
    //    printf("In phase 3 (recvs) on iter %d\n", i);
    //    fflush(stdout);
    if (outstanding_receive) { /* if we're currently waiting on a message */
      /* if we've finished receiving the message, move on... else keep waiting */
      __hpx_network_ops->sendrecv_test(&recv_request, &curr_flag, &curr_status);

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
	hpx_parcel_deserialize(recv_buffer, &parcel);
	/* Right now, parcel_deserialize() calls hpx_alloc(). In the
	   future, if that changes, we might have to allocate a new buffer
	   here.... */


	/* invoke action */
	if (parcel->action.action != NULL) {
	  // printf("Invoking action %s\n", parcel->action.name);
	  success = hpx_action_invoke(&(parcel->action), parcel->payload, &result);
	}
	/* TODO: deal with case where there is no invocation */
      } /* end if(curr_flag) */
    }
    else { /* otherwise, see if there's a message to wait for */
      __hpx_network_ops->probe(NETWORK_ANY_SOURCE, &curr_flag, &curr_status);
      if (curr_flag) { /* there is a message to receive */
	#if DEBUG
          initiated_something = 1;
	  probe_successes++;
	#endif
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
    if (quitting && request_queue_empty(&network_send_requests) && !outstanding_receive)
      break;

    i++;

    /* If we don't yield occasionally, any thread that get scheduled to this core will get stuck. TODO: takeout after resolving this */
    if (i % 1000 == 0)
      hpx_thread_yield();



    //printf("Parcel handler _main spinning i = %zd\n", i);
  }

  #if DEBUG
  printf("Handler done after iter %d\n", (int)i);
    fflush(stdout);
  #endif

  free(recv_buffer);

 error:
  hpx_thread_exit((void*)retval);

  return NULL;

}

hpx_parcelhandler_t * hpx_parcelhandler_create(hpx_context_t *ctx) {
  int ret = HPX_ERROR;
  hpx_config_t * cfg = NULL;
  hpx_parcelhandler_t * ph = NULL;

  /* create and initialize queue */
  ret = hpx_parcelqueue_create(&__hpx_parcelqueue_local);
  if (ret != 0) {
    __hpx_errno = HPX_ERROR;
    goto error;
  }

  /* create and initialize send queue */
  ret = hpx_parcelqueue_create(&__hpx_send_queue);
  if (ret != 0) {
    __hpx_errno = HPX_ERROR;
    goto error;
  }

  /* create and initialize private network queues */

  ret = request_queue_init(&network_send_requests, REQUEST_QUEUE_SIZE);
  if (ret != 0) {
    ret = HPX_ERROR_NOMEM;
    __hpx_errno = HPX_ERROR_NOMEM;
    goto error;
  }
  ret = request_queue_init(&network_recv_requests, REQUEST_QUEUE_SIZE);
  if (ret != 0) {
    ret = HPX_ERROR_NOMEM;
    __hpx_errno = HPX_ERROR_NOMEM;
    goto error;
  }
  ret = request_queue_init(&network_put_requests, REQUEST_QUEUE_SIZE);
  if (ret != 0) {
    ret = HPX_ERROR_NOMEM;
    __hpx_errno = HPX_ERROR_NOMEM;
    goto error;
  }
  ret = request_queue_init(&network_get_requests, REQUEST_QUEUE_SIZE);
  if (ret != 0) {
    ret = HPX_ERROR_NOMEM;
    __hpx_errno = HPX_ERROR_NOMEM;
    goto error;
  }

  /* create thread */
  ph = hpx_alloc(sizeof(hpx_parcelhandler_t));
  
  ph->ctx = ctx;
  ph->thread = hpx_thread_create(ph->ctx, HPX_THREAD_OPT_SERVICE_COREGLOBAL, (hpx_func_t)_hpx_parcelhandler_main, 0);
  /* TODO: error check */

 error:
  return ph;
}

void hpx_parcelhandler_destroy(hpx_parcelhandler_t * ph) {
  MPI_Barrier(MPI_COMM_WORLD);

  int *child_retval;

  hpx_parcel_t* kill_parcel;
  /* could possibly use hpx_send_parcel instead */
  kill_parcel = hpx_alloc(sizeof(hpx_parcel_t));
  if (kill_parcel == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    goto error;
  }
  kill_parcel->action.action = (hpx_func_t)_HPX_PARCELHANDLER_KILL_ACTION;
  hpx_parcelqueue_push(__hpx_parcelqueue_local, (void*)kill_parcel);

  hpx_thread_join(ph->thread, (void**)&child_retval);

  /* test code */
  /* TODO: remove */
//  printf("Thread joined\n");
//  printf("Value on return = %d\n", *child_retval);
  /* end test code */

  // hpx_thread_destroy(ph->thread);
  hpx_free(ph);

  request_queue_destroy(&network_send_requests);
  request_queue_destroy(&network_recv_requests);
  request_queue_destroy(&network_put_requests);
  request_queue_destroy(&network_get_requests);

  hpx_parcelqueue_destroy(&__hpx_send_queue);
  hpx_parcelqueue_destroy(&__hpx_parcelqueue_local);

 error:
  return;
}
