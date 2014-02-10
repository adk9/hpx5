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

#include "parcelhandler.h"                      /* public interface */
#include "hpx/error.h"
#include "hpx/globals.h"                        /* __hpx_network_ops */
#include "hpx/lco.h"                            /* hpx_future_t */
#include "hpx/parcel.h"                         /* hpx_parcel_* */
#include "hpx/thread/ctx.h"                     /* hpx_context_t */
#include "block.h"                              /* parcel_block_t and get_block()  */
#include "debug.h"                              /* dbg_ routines */
#include "hashstr.h"                            /* hashstr() */
#include "network.h"                            /* struct network_status */
#include "parcel.h"                             /* struct hpx_parcel */
#include "parcelqueue.h"                        /* struct parcelqueue */
#include "request_buffer.h"                     /* struct request_buffer */
#include "request_list.h"                       /* struct request_list */

// #define HPX_PARCELHANDLER_GET_THRESHOLD 0 // force all payloads to be sent via put/get
/* At present, the threshold is used with the payload_size. BUT doesn't it make more sense to compare against total parcel size? The action name factors in then... */
#define HPX_PARCELHANDLER_GET_THRESHOLD SIZE_MAX /* SIZE_MAX ensures all parcels go through send/recv not put/get */

/* TODO: make configurable (and find a good, sane size) */
static const size_t REQUEST_BUFFER_SIZE     = 2048;
/* TODO: make configurable (and use a real size) */ 
static const size_t RECV_BUFFER_SIZE        = 1024*1024*16; 

/**
 * The send queue for this locality.
 */
struct parcelqueue *__hpx_send_queue = NULL;

/**
 * The parcel handler structure.
 *
 * This is what is returned by parcelhandler_create().
 */
typedef struct parcelhandler {
  hpx_context_t        *ctx;
  struct hpx_thread *thread;  
  hpx_future_t        *quit;              /*!< signals parcel handler to quit */
  hpx_future_t         *fut;
} parcelhandler_t;

/* Check if the block this parcel belongs to is pinned, and if not, pin it */
/* BDM At present, this is done by the parcel handler. For a variety
of reasons, including performance and simplicity, it might be
preferable to do this elsewhere (e.g. when parcels are
allocated). HOWEVER, network backends that both (1) require
pinning/registration and (2) are not thread safe will blow up. */ 
static void 
pin_if_necessary(struct hpx_parcel_t* parcel) 
{
  /* pin this parcel's block, if necessary */
  parcel_block_t *block = get_block(parcel);
  if (block->header.pinned == false) {
    int block_size = get_block_size(block);
    __hpx_network_ops->pin(block, block_size);
  }
  return;
}

/**
 * Take a header from the network and create a local thread to process it.
 *
 * @param[in] header - the header to process
 *
 * @returns HPX_SUCCESS or an error code
 */
static hpx_error_t
complete(struct hpx_parcel* header, bool send)
{
  dbg_assert_precondition(header);

  /* just free the header on a send completion */
  if (send) {
    hpx_parcel_release(header);
    return HPX_SUCCESS;
  }

  dbg_printf("%d: Received %d bytes to buffer at %p "
             "action=%" HPX_PRIu_hpx_action_t "\n",
             hpx_get_rank(), header->size, (void*)header,
             header->action);

  return hpx_action_invoke_parcel(header, NULL);
}

typedef int(*test_function_t)(network_request_t*, int*, network_status_t*);

static int
complete_requests(request_list_t* list, test_function_t test, bool send)
{
  int count = 0;

  request_list_begin(list);
  network_request_t* req;                       /* loop iterator */
  int flag;                                     /* used in test */
  while ((req = request_list_curr(list)) != NULL) {
    int success = test(req, &flag, NULL);
    if (flag == 1 && success == 0) {
      struct hpx_parcel* header = request_list_curr_parcel(list);
      request_list_del(list);
      dbg_check_success(complete(header, send));
      ++count;
    } 
    request_list_next(list);
  }
  return count;
}

static void
parcelhandler_main(parcelhandler_t *args)
{
  hpx_waitfor_action_registration_complete(); /* make sure action registration is done or else the parcel handler can't invoke actions */

  int success;

  hpx_future_t* quit = args->quit;

  request_list_t send_requests;
  request_list_t recv_requests;
  request_list_init(&send_requests);
  request_list_init(&recv_requests);

  struct hpx_parcel* header;
  size_t i;
  int completions;

  network_request_t* req;

  size_t probe_successes, recv_successes, send_successes;
  int initiated_something, completed_something;

  int dst_rank; /* raw network address as opposed to internal hpx address */

  int outstanding_recvs;
  int outstanding_sends;

  hpx_parcel_t *recv_buffer;
  size_t recv_size;
  int *retval;
  int flag;
  network_status_t status;

  outstanding_recvs = 0;
  outstanding_sends = 0;
  retval = hpx_alloc(sizeof(int));
  *retval = 0;

  i = 0;

  if (HPX_DEBUG) {
    probe_successes= 0;
    recv_successes = 0;
    send_successes = 0;
    initiated_something = 0;
    completed_something = 0;
  }

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
      if (HPX_DEBUG && (completions > 0)) {
        completed_something = 1;
        send_successes++;
      }
    } /* if (outstanding_sends > 0) */
    
    /* check send queue */
    header = parcelqueue_trypop(__hpx_send_queue);
    if (header != NULL) {
      if (HPX_DEBUG) {
        initiated_something = 1;
      }
      if (header == NULL) {
        /* TODO: signal error to somewhere else! */
      }
      pin_if_necessary(header);
      /* now send */
      dst_rank = header->dest.locality.rank;
      dbg_printf("%d: Sending %d bytes from buffer at %p with "
                 "action=%" HPX_PRIu_hpx_action_t "\n",
                 hpx_get_rank(), parcel_size(header), (void*)header, header->action);
      req = request_list_append(&send_requests, header);
      dbg_printf("%d: Sending with request at %p from buffer at %p\n",
                 hpx_get_rank(), (void*)req, (void*)header); 
      __hpx_network_ops->send(dst_rank, 
                              header,
                              parcel_size(header),
                              req);
      outstanding_sends++;
    }

    /* ==================================
       Phase 2: Deal with remote parcels 
       ==================================
    */
    if (outstanding_recvs > 0) {
      completions = complete_requests(&recv_requests, __hpx_network_ops->recv_test, false);
      outstanding_recvs -= completions;
      if (HPX_DEBUG && (completions > 0)) {
        completed_something = 1;
        recv_successes += completions;
      }
    }
  
    /* Now check for new receives */
    success = __hpx_network_ops->probe(NETWORK_ANY_SOURCE, &flag, &status);
    if (success == 0 && flag > 0) { /* there is a message to receive */
      if (HPX_DEBUG) {
        initiated_something = 1;
        probe_successes++;
      }
      else {
	/* TODO: handle error */
      }
    
      recv_size = status.count;
      recv_buffer = hpx_parcel_acquire(recv_size - sizeof(struct hpx_parcel));
      if (recv_buffer == NULL) {
        __hpx_errno = HPX_ERROR_NOMEM;
        *retval = HPX_ERROR_NOMEM;
        goto error;
      } 
      pin_if_necessary(recv_buffer);
      dbg_printf("%d: Receiving %zd bytes to buffer at %p\n", hpx_get_rank(),
                 recv_size, (void*)recv_buffer); 
      req = request_list_append(&recv_requests, recv_buffer);
      __hpx_network_ops->recv(status.source, recv_buffer, recv_size, req);
      outstanding_recvs++;
    }
  
  
    if (HPX_DEBUG) {
      if (initiated_something != 0 || completed_something != 0) {
        dbg_printf("rank %d: initiated: %d\tcompleted "
                   "%d\tprobes=%d\trecvs=%d\tsend=%d\n", hpx_get_rank(),
                   initiated_something, 
                   completed_something, (int)probe_successes, (int)recv_successes, 
                   (int)send_successes); 
        initiated_something = 0;
        completed_something = 0;
      }
    }
  
  if (hpx_lco_future_isset(quit) == true && outstanding_sends == 0 && outstanding_recvs == 0 && parcelqueue_empty(__hpx_send_queue))
      break;
    /* If we don't yield occasionally, any thread that get scheduled to this core will get stuck. */
    i++;
    if (i % 1000 == 0)
      hpx_thread_yield();    
  }
  
  dbg_printf("%d: Handler done after iter %d\n", hpx_get_rank(), (int)i);

error:  
  hpx_thread_exit((void*)retval);
}

parcelhandler_t *
parcelhandler_create(hpx_context_t *ctx)
{
  int ret = HPX_ERROR;
  parcelhandler_t *ph = NULL;

  /* create and initialize send queue */
  ret = parcelqueue_create(&__hpx_send_queue);
  if (ret != 0) {
    __hpx_errno = HPX_ERROR;
    return NULL;
  }

  /* create thread */
  ph = hpx_alloc(sizeof(*ph));
  ph->ctx = ctx;
  ph->quit = hpx_alloc(sizeof(hpx_future_t));
  hpx_lco_future_init(ph->quit);
  hpx_error_t e = hpx_thread_create(ph->ctx, HPX_THREAD_OPT_SERVICE_COREGLOBAL,
                                    (hpx_func_t)parcelhandler_main,
                                    (void*)ph,
                                    &ph->fut,
                                    &ph->thread);
  if (e == HPX_ERROR)
    dbg_print_error(e, "Failed to start the parcel handler core service");
  
  return ph;
}

void
parcelhandler_destroy(parcelhandler_t *ph)
{
  if (!ph)
    return;
  
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

/**
 *
 */
int
parcelhandler_send(hpx_locality_t *dest,
                   struct hpx_parcel *parcel,
                   hpx_future_t *complete,
                   hpx_future_t *thread,
                   hpx_future_t **result)
{
  /* need this hack for now, because we don't have global addresses */
  parcel->dest.locality = *dest;
  
  int e = parcelqueue_push(__hpx_send_queue, parcel);
  if (e != HPX_SUCCESS) {
    dbg_print_error(e, "Failed to add a parcel to the send queue");
    __hpx_errno = e;
    return e;
  }

  /* TODO FIXME: where are futures for complete and thread??? */

  return HPX_SUCCESS;
}
