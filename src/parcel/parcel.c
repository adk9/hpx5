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

hpx_error_t hpx_parcel_init(void) {
    int status;
    hpx_error_t ret;

    /* create an action table */
    
    bzero(&action_table, sizeof(action_table));
    
    /* ADK: TODO: we need a better hash table implementation. */
    status = hcreate_r(ACTION_TABLE_SIZE, &action_table);
    

    if (status == 0) /* this should be impossible */
      /* BDM: note that hcreate_r returns 0 on FAILURE, and non-zero on success. backwards, but hey */
      return HPX_ERROR;
    
    /* spawn the parcel handler/dispatcher thread */

    ret = HPX_SUCCESS;
    return ret;
}


void hpx_parcel_fini(void) {
    /* destroy the action table */
    hdestroy_r(&action_table);
  
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
hpx_error_t hpx_new_parcel(char *act, void* args, size_t len, hpx_parcel_t *handle) {
  hpx_error_t ret;
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

  handle->payload = args;
  handle->payload_size = len;

  ret = HPX_SUCCESS;
 error:
  return ret;
}

hpx_thread_t *hpx_call(hpx_locality_t *dest, char *action,
                       void *args, size_t len) {
  hpx_error_t ret;
  hpx_parcel_t p;
  /* create a parcel from action, args, len */
  hpx_new_parcel(action, args, len, &p);
  /* send parcel to the destination locality */
  ret = hpx_send_parcel(dest, &p);

  return NULL; /* TODO */
}

/**
   Caller is responsible for freeing *blob.
   Will return HPX_ERROR if (1) payload size is not 0 and (2) payload is NULL (a NULL pointer is allowed if payload size is 0)
*/
hpx_error_t hpx_parcel_serialize(hpx_parcel_t *p, char** blob) {
  /* TODO: do something better than using char* for everything? maybe a struct? and make sure alignments are better taken care of */
  /* TODO: check size? */
  hpx_error_t ret;
  size_t size_of_action_name;
  size_t size_of_payload;
  size_t size_of_blob;
  size_t blobi; /* where we are at in the blob */

  blobi = 0;
  ret = HPX_ERROR;

  /* sanity check parcel to catch a common mistake */
  if (p->payload_size != 0 && p->payload == NULL) {
    __hpx_errno = HPX_ERROR;
    return HPX_ERROR;
  }
    

  /* figure out how much space we need */
  size_of_payload = p->payload_size;
  size_of_action_name = sizeof(char)*(strlen(p->action.name) + 1);
  size_of_blob = sizeof(hpx_parcel_t) + size_of_action_name + size_of_payload;

  /* allocate space for binary blob */
  *blob = hpx_alloc(size_of_blob);
  if (*blob == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    ret = HPX_ERROR_NOMEM;
    goto error;
  }

  /* copy the parcel struct to the blob */
  memcpy(*blob, (void*)p, sizeof(hpx_parcel_t));
  blobi += sizeof(hpx_parcel_t);

  /* copy the action name to the blob */
  strncpy(*blob + blobi, p->action.name, size_of_action_name);
  blobi += size_of_action_name; /* We have ALREADY included +1 for nul terminator */
  (*blob)[blobi - 1] = '\0'; /* just in case some other thread did something very bad, we will limit the damage here */

  /* copy the payload to the blob */
  if (size_of_payload > 0)
    memcpy(*blob + blobi, (char*)p->payload, size_of_payload);
  blobi += size_of_payload;

  ret = HPX_SUCCESS;
 error:
  return ret;
}

/* DO NOT FREE THE RETURN VALUE */
/* FIXME CAUTION for some reason using this causes a bug (alisaing issues?). gcc interprets the return value as a 32 bit integer and sign extends it to 64 bits resulting in BAD THINGS happening... */
hpx_parcel_t* hpx_read_serialized_parcel(char* blob) {
  return (hpx_parcel_t*)blob;
}

/** caller is reponsible for free()ing *p and *p->payload */
hpx_error_t hpx_parcel_deserialize(char* blob, hpx_parcel_t** p) {
  hpx_error_t ret;
  size_t size_of_action_name;
  size_t size_of_payload;
  size_t size_of_blob;
  size_t blobi; /* where we are at in the blob */
  void* payload;

  blobi = 0;
  ret = HPX_ERROR;

  /* we know we need room for the parcel so make room for that */
  *p = hpx_alloc(sizeof(hpx_parcel_t));
  if (*p == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    ret = HPX_ERROR_NOMEM;
    goto error;
  }
  memcpy((char*)*p, blob, sizeof(hpx_parcel_t));
  blobi += sizeof(hpx_parcel_t);

  /* now we can figure out the size of our payload */
  size_of_payload = ((hpx_parcel_t*)blob)->payload_size;

  /* now we can figure out the size of our action name */
  size_of_action_name = sizeof(char) * strlen((char*)blob + blobi) + 1;
  
  /* allocate space for payload */
  payload = hpx_alloc(size_of_payload);
  if (payload == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    ret = HPX_ERROR_NOMEM;
    free(*p);
    goto error;
  }

  /* lookup our local action - that way we avoid problems with who is responsible for free()ing action.name */
  hpx_action_lookup_local((char*)blob + blobi, &((*p)->action));
  blobi += size_of_action_name;
  
  /* move payload to new payload space */
  if (size_of_payload > 0)
    memcpy((char*)payload, blob + blobi, size_of_payload);
  blobi += size_of_payload;

  /* fix up our payload pointer in our parcel */
  (*p)->payload = payload;

  ret = HPX_SUCCESS;
 error:
  return ret;
}

hpx_error_t hpx_send_parcel(hpx_locality_t * loc, hpx_parcel_t *p) {
  hpx_error_t ret;
  char* serialized_parcel;
  ret = HPX_ERROR;

  if (loc->rank != hpx_get_rank()) {
    p->dest.locality = *loc;
    ret = hpx_parcel_serialize(p, &serialized_parcel);
    if (ret != 0) {   
      __hpx_errno = ret;
      return ret;
    }
    
    ret = hpx_parcelqueue_push(__hpx_send_queue, (void*)serialized_parcel);
    if (ret != 0) {   
      __hpx_errno = ret;
      return ret;
    }
  }
  else {
    hpx_action_invoke(&p->action, p->payload, NULL);
    free(p);
  }  

  ret = HPX_SUCCESS;
  return ret;
}
