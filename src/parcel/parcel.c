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

#define _GNU_SOURCE 
#include <stdlib.h>
#include <search.h>

#include "hpx/action.h"
#include "hpx/error.h"
#include "hpx/network.h"
#include "hpx/parcel.h"
#include "hpx/parcelhandler.h"

struct hsearch_data action_table;

int hpx_parcel_init(void) {
    int status;

    /* create an action table */
    
    bzero(&action_table, sizeof(action_table));
    
    /* ADK: TODO: we need a better hash table implementation. */
    status = hcreate_r(ACTION_TABLE_SIZE, &action_table);
    
    if (status != 0) /* this should be impossible */
      return HPX_ERROR;
    
    /* spawn the parcel handler/dispatcher thread */
}


void hpx_parcel_fini(void) {
    /* destroy the action table */
    hdestroy_r(&action_table);
  
    /* shutdown the parcel handler thread */
  
}

/*
 --------------------------------------------------------------------
  hpx_new_parcel

  Create a new parcel. We pass in an action @act, instead of the
  action's name so that we can save the lookup cost at the remote
  locality if we are running under the SPMD/symmetric heap assumption.
  -------------------------------------------------------------------
*/
int hpx_new_parcel(char *act, void* args, size_t len, hpx_parcel_t *handle) {
  int ret;
  int temp;
  ret = HPX_ERROR;

  /* where do args go? to what does len refer to (i.e. args or data or either)? */
  /* I'm going to assume args is args or payload, depending
     Also, I'm going to assume new_parcel doesn't allocate it... */

  temp = hpx_action_lookup_local(act, &(handle->action));
  if (temp != 0) {
    __hpx_errno = HPX_ERROR;
    ret = temp;
    goto error;
  }

  /* TODO: set parcel_id */

  /* put this back in if we're responsibe for allocating room for data
  handle->payload = hpx_alloc(len);
  if (handle->payload == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    ret = HPX_ERROR_NOMEM;
    goto error;
  }
  */

  ret = 0;
 error:
  return ret;
}

hpx_thread_t *hpx_call(hpx_locality_t *dest, char *action,
                       void *args, size_t len) {
  int ret;
  hpx_parcel_t p;
  /* create a parcel from action, args, len */
  hpx_new_parcel(action, args, len, &p);
  /* send parcel to the destination locality */
  ret = hpx_send_parcel(dest, &p);

  return NULL; /* TODO */
}

/* caller is responsible for freeing *blob */
int hpx_serialize_parcel(hpx_parcel_t *p, char** blob) {
  /* TODO: check size? */
  int ret;
  ret = HPX_ERROR;
  *blob = hpx_alloc(sizeof(hpx_parcel_t) + p->payload_size);
  if (*blob == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    ret = HPX_ERROR_NOMEM;
    goto error;
  }

  memcpy(*blob, (void*)p, sizeof(hpx_parcel_t));
  memcpy(*blob + sizeof(hpx_parcel_t), (void*)p, p->payload_size);

  ret = 0;
 error:
  return ret;
}

/* DO NOT FREE THE RETURN VALUE */
hpx_parcel_t* hpx_read_serialized_parcel(char* blob) {
  return (hpx_parcel_t*)blob;
}

/* caller is reponsible for free()ing *p and *p->payload */
int hpx_deserialize_parcel(char* blob, hpx_parcel_t** p) {
  int ret;
  size_t payload_size;
  void* data;

  ret = HPX_ERROR;
  payload_size = 0;

  *p = hpx_alloc(sizeof(hpx_parcel_t));
  if (*p == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    ret = HPX_ERROR_NOMEM;
    goto error;
  }
  payload_size = ((hpx_parcel_t*)blob)->payload_size;
  
  if (payload_size > 0) {
    data = hpx_alloc(payload_size);
    if (data == NULL) {
      __hpx_errno = HPX_ERROR_NOMEM;
      ret = HPX_ERROR_NOMEM;
      goto error;
    } 
    memcpy(data, blob + sizeof(hpx_parcel_t), payload_size);
  }
  else
    data = NULL;

  (*p)->payload = data;

  memcpy(*p, blob, sizeof(hpx_parcel_t));

  ret = 0;
 error:
  return ret;
}

int hpx_send_parcel(hpx_locality_t * loc, hpx_parcel_t *p) {
  int ret;
  char* serialized_parcel;

  p->dest.locality = *loc;
  hpx_serialize_parcel(p, &serialized_parcel);
  ret = hpx_parcelqueue_push(__hpx_send_queue, (void*)serialized_parcel);
  if (ret != 0) {   
    __hpx_errno = ret;
  }

  return ret;
}
