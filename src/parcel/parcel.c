/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Parcel Registration
  register.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include "serialization.h"
#include "parcelqueue.h"                        /* __send_queue */
#include "network.h"
#include "hpx/action.h"
#include "hpx/error.h"
#include "hpx/parcel.h"

hpx_error_t hpx_parcel_init(void) {
  /* LD: action table is initialized lazilty, nothing to do */
  return HPX_SUCCESS;
}


void hpx_parcel_fini(void) {
  /* destroy the action table */
  /* shutdown the parcel handler thread */
}

/**
 --------------------------------------------------------------------
  hpx_new_parcel

  Create a new parcel. We pass in an action @act, instead of the
  action's name so that we can save the lookup cost at the remote
  locality if we are running under the SPMD/symmetric heap assumption.
  -------------------------------------------------------------------
*/
hpx_error_t hpx_new_parcel(hpx_action_t act, void* args, size_t len,
                           hpx_parcel_t *out) {

  /* where do args go? to what does len refer to (i.e. args or data or either)? */
  /* I'm going to assume args is args or payload, depending
     Also, I'm going to assume new_parcel doesn't allocate it... */

  
  /* TODO: set parcel_id */

  /* put this back in if we're responsibe for allocating room for data
  handle->payload = hpx_alloc(len);
  if (handle->payload == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    ret = HPX_ERROR_NOMEM;
    goto error;
  }
  */
  out->parcel_id = 0;
  out->action = act;
  out->payload = args;
  out->payload_size = len;

  return HPX_SUCCESS;
}

hpx_thread_t *hpx_call(hpx_locality_t *dest, hpx_action_t action,
                       void *args, size_t len) {
  hpx_parcel_t *p = hpx_alloc(sizeof(*p));
  /* create a parcel from action, args, len */
  hpx_new_parcel(action, args, len, p);
  /* send parcel to the destination locality */
  hpx_send_parcel(dest, p);

  return NULL; /* TODO */
}

hpx_error_t hpx_send_parcel(hpx_locality_t * loc, hpx_parcel_t *p) {
  hpx_error_t ret;
  struct header* serialized_parcel;
  ret = HPX_ERROR;

  if (loc->rank != hpx_get_rank()) {
    p->dest.locality = *loc;
    ret = serialize(p, &serialized_parcel);
    if (ret != 0) {   
      __hpx_errno = ret;
      return ret;
    }
    
    ret = parcelqueue_push(__hpx_send_queue, serialized_parcel);
    if (ret != 0) {   
      __hpx_errno = ret;
      return ret;
    }
  }
  else {
    hpx_action_invoke(p->action, p->payload, NULL);
    hpx_free(p);
  }  

  ret = HPX_SUCCESS;
  return ret;
}
