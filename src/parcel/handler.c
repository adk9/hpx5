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

#define _HPX_PARCELHANDLER_KILL_ACTION 0xdead

/* 
   Parcel Queue

   The handler needs an efficient way to get parcels from other
   threads. This queue is designed to meet that goal. The design goals
   for the queue were:
   
   it must be safe despite being read to by many threads,
   
   it must be efficient for the consumer (the parcel handler) since it
   reads from it very often,
   
   it should be as relatively efficient for the producers.

   It is a locking queue for simplicity. Fortunately, as there is only
   one consumer, we only need a lock on the tail and not one on the
   head. Like in Michael & Scott, we use dummy nodes so that proucers
   and the consumer do not block each other. This is especially
   helpful in our case, since pop is supposed to be fast.

   (In fact, in the final implementation, it pretty much is the
   two-lock queue from Michael & Scott with no head lock. See
   http://www.cs.rochester.edu/research/synchronization/pseudocode/queues.html)

   One known issue with the current queue implementation is I that one
   thread being killed or dying while pushing to the queue can lock up
   the queue. Is that possible at present? So it may not actually be
   that safe...

*/

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

void * _hpx_parcelhandler_main_pingpong(void) {
#if HAVE_MPI
  int PING_TAG=0;
  int PONG_TAG=1;
  fflush(NULL);
  int size;
  int rank;
  int next, prev;
  int success;
  int flag;
  int type;
  MPI_Status stat;
  int i;
  int iter_limit=10;//1000;
  int buffer_size = 1024*1024;
  char* send_buffer = NULL;
  char* recv_buffer = NULL;
  char* copy_buffer = NULL;
  uint32_t send_request;
  uint32_t recv_request;

  struct timespec begin_ts;
  struct timespec end_ts;
  unsigned long elapsed;
  double avg_oneway_latency;

  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  // use MPI ==================================================================================================================
  send_buffer = (char*)malloc(buffer_size*sizeof(char));
  recv_buffer = (char*)malloc(buffer_size*sizeof(char));
  copy_buffer = (char*)malloc(buffer_size*sizeof(char));

  clock_gettime(CLOCK_MONOTONIC, &begin_ts);

  if (rank == 0) {
    for (i = 0; i < iter_limit; i++) {
      snprintf(send_buffer, 50, "Message %d from proc 0", i);
      MPI_Send(send_buffer, buffer_size, MPI_CHAR, 1, PING_TAG, MPI_COMM_WORLD);
      
      MPI_Recv(recv_buffer, buffer_size, MPI_CHAR, 1, PONG_TAG, MPI_COMM_WORLD, &stat);
      // printf("Message from proc 1: <<%s>>\n", recv_buffer);
    }
  }
  else if (rank == 1) {
    strcpy(send_buffer, "Message from proc 1");

    for (i = 0; i < iter_limit; i++) {
      MPI_Recv(recv_buffer, buffer_size, MPI_CHAR, 0, PING_TAG, MPI_COMM_WORLD, &stat);

      int str_length;
      snprintf(copy_buffer, 50, "At %d, received from proc 0 message: '", i);
      str_length = strlen(copy_buffer);
      strcpy(&copy_buffer[str_length], recv_buffer);
      str_length = strlen(copy_buffer);
      strcpy(&copy_buffer[str_length], "'");
      strcpy(send_buffer, copy_buffer);

      MPI_Send(send_buffer, buffer_size, MPI_CHAR, 0, PONG_TAG, MPI_COMM_WORLD);
    }
  }

  clock_gettime(CLOCK_MONOTONIC, &end_ts);
  elapsed = ((end_ts.tv_sec * 1000000000) + end_ts.tv_nsec) - ((begin_ts.tv_sec * 1000000000) + begin_ts.tv_nsec);

  avg_oneway_latency = elapsed/((double)(iter_limit*2));

  printf("average oneway latency (MPI):   %f ms\n", avg_oneway_latency/1000000.0);

  free(send_buffer);
  free(recv_buffer);

  // end of use MPI part ===========================================================================================

#if HAVE_PHOTON
  send_buffer = (char*)malloc(buffer_size*sizeof(char));
  recv_buffer = (char*)malloc(buffer_size*sizeof(char));
  copy_buffer = (char*)malloc(buffer_size*sizeof(char));

  hpx_network_register_buffer(send_buffer, buffer_size);
  hpx_network_register_buffer(recv_buffer, buffer_size);

  clock_gettime(CLOCK_MONOTONIC, &begin_ts);

  if (rank == 0) {

    for (i = 0; i < iter_limit; i++) {
      snprintf(send_buffer, 50, "Message %d from proc 0", i);
      hpx_network_send_start(1, PING_TAG, send_buffer, buffer_size, 0, &send_request);
      hpx_network_wait(send_request);
      
      //      photon_wait_send_request_rdma(PONG_TAG);
      hpx_network_recv_start(-1, PONG_TAG, recv_buffer, buffer_size, 0, &recv_request);
      //hpx_network_recv_start(1, PONG_TAG, recv_buffer, buffer_size, 0, &recv_request);
      hpx_network_wait(recv_request);
      //printf("%02d: Receiver done waiting on recv %d\n", i, recv_request);
      //printf("Message from proc 1: <<%s>>\n", recv_buffer);


    }
  }
  else if (rank == 1) {
    //    strcpy(send_buffer, "Message from proc 1");


    for (i = 0; i < iter_limit; i++) {
      hpx_network_recv_start(0, PING_TAG, recv_buffer, buffer_size, 0, &recv_request);
      hpx_network_wait(recv_request);

      int str_length;
      snprintf(copy_buffer, 50, "At %d, received from proc 0 message: '", i);
      str_length = strlen(copy_buffer);
      strcpy(&copy_buffer[str_length], recv_buffer);
      str_length = strlen(copy_buffer);
      strcpy(&copy_buffer[str_length], "'");
      strcpy(send_buffer, copy_buffer);

      photon_post_send_request_rdma(0, buffer_size, PONG_TAG, &send_request);
      hpx_network_wait(send_request);
      //printf("%02d: Sender done waiting on photon_post_send_request_rdma %d\n", i, send_request);
      hpx_network_send_start(0, PONG_TAG, send_buffer, buffer_size, 0, &send_request);
      hpx_network_wait(send_request);
      //printf("%02d: Sender done waiting on send %d\n", i, send_request);
    }
  }

  clock_gettime(CLOCK_MONOTONIC, &end_ts);
  elapsed = ((end_ts.tv_sec * 1000000000) + end_ts.tv_nsec) - ((begin_ts.tv_sec * 1000000000) + begin_ts.tv_nsec);

  avg_oneway_latency = elapsed/((double)(iter_limit*2));

  printf("average oneway latency :   %f ms\n", avg_oneway_latency/1000000.0);

  hpx_network_unregister_buffer(send_buffer, buffer_size);
  hpx_network_unregister_buffer(recv_buffer, buffer_size);

  free(send_buffer);
  free(recv_buffer);
  free(copy_buffer);
#endif // end of if HAVE_PHOTON

#endif // end of if HAVE_MPI

  while (0) {
    /* TODO: wait for signal to exit... */
  }

  int * retval;
  retval = hpx_alloc(sizeof(int));
  *retval = 0;
  hpx_thread_exit((void*)retval);

  return NULL;
}

void * _hpx_parcelhandler_main_dummy(void) {

  size_t i = 0;
  while (1) {
    sleep(1);
    printf("Parcel handler spinning i = %zd\n", i);
    i++;
  }

  int * retval;
  retval = hpx_alloc(sizeof(int));
  *retval = 0;
  hpx_thread_exit((void*)retval);

  return NULL;
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
void * _hpx_parcelhandler_main(void) {
  bool quitting;
  int success;
  hpx_parcel_t* parcel;
  void* result; /* for action returns */
  size_t i;
  size_t probe_successes, recv_successes;

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
  probe_successes= 0;
  recv_successes = 0;
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
      if (curr_flag)
	request_queue_pop(&network_send_requests);
    }

    /* check send queue */
    if (!request_queue_full(&network_send_requests)) { /* don't do this if we can't handle any more send requests */
      parcel_data = hpx_parcelqueue_trypop(__hpx_send_queue);
      if (parcel_data != NULL) {
	/* first sizeof(hpx_parcel_t) bytes of a serialized parcel is
	   actually a hpx_parcel_t: */
	//	parcel = hpx_read_serialized_parcel(parcel_data); 
	parcel = (hpx_parcel_t*)parcel_data;
	if (parcel == NULL) {
	  /* TODO: signal error to somewhere else! */
	}
	network_rank = parcel->dest.locality.rank;
	network_size = sizeof(hpx_parcel_t) + parcel->payload_size; /* total size is parcel header size + data: */
	if(1) { 	/* TODO: check if size is over the eager limit, then use put() instead */
	  __hpx_network_ops->send(network_rank, 
				  parcel_data, 
				  sizeof(hpx_parcel_t) + parcel->payload_size,
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
      if (parcel->action.action == (hpx_func_t)_HPX_PARCELHANDLER_KILL_ACTION) {
	quitting = true;
	// break;
      }
      else {
	/* invoke action */
	if (parcel->action.action != NULL) {
	  success = hpx_action_invoke(&(parcel->action), NULL, &result);
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
	recv_successes++;
	outstanding_receive = false;
	/* If we've received something, do stuff:
	   * If it's a notificiation of a put, call get() TODO: do this
	   * If it's an action invocation, do that.
	*/

	/* TODO: move this to a seperate thread */
	hpx_deserialize_parcel(recv_buffer, &parcel);

	/* invoke action */
	if (parcel->action.action != NULL) {
	  // printf("Invoking action %s\n", parcel->action.name);
	  success = hpx_action_invoke(&(parcel->action), NULL, &result);
	}
	/* TODO: deal with case where there is no invocation */
      } /* end if(curr_flag) */
    }
    else { /* otherwise, see if there's a message to wait for */
      probe_successes++;
      __hpx_network_ops->probe(NETWORK_ANY_SOURCE, &curr_flag, &curr_status);
      if (curr_flag) { /* there is a message to receive */
	__hpx_network_ops->recv(curr_status.source, recv_buffer, NETWORK_ANY_LENGTH, &recv_request);
	outstanding_receive = true;	
      }
      
    } /* end if(outstanding_receive) */
    /* now get a new parcel to process */
    
    /* Right now, deserialize_parcel() calls hpx_alloc(). In the
       future, if that changes, we might have to allocate a new buffer
       here.... */

    /* TODO: if no sends in queue, nothing with probe, and quitting == true, break */
    if (quitting && request_queue_empty(&network_send_requests) && !outstanding_receive)
      break;

    i++;
    //printf("Parcel handler _main spinning i = %zd\n", i);
  }

  //  printf("Handler done after iter %d\n", i);
  //  fflush(stdout);


  //printf("Parcel handler _main exiting\n");

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
