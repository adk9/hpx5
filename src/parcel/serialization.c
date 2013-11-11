/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  ParcelQueue Functions
  src/parcel/serialization.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
  Benjamin D. Martin <benjmart [at] indiana.edu>
  Luke Dalessandro   <ldalessa [at] indiana.edu>
  ====================================================================
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stddef.h>                             /* size_t */
#include <stdint.h>                             /* uint8_t */
#include <string.h>                             /* memcpy */
#if DEBUG_HANDLER
#include <stdio.h>
#endif

#include "serialization.h"
#include "hpx/mem.h"                            /* hpx_{alloc,free} */
#include "hpx/parcel.h"                         /* struct parcel */
#include "network.h"                    /* FOURBYTE_ALIGN */

extern network_ops_t *__hpx_network_ops;

inline size_t get_parcel_size(struct header* header) {
  size_t size;
  size = sizeof(*header); /* total size is parcel header size + data: */
  size += header->payload_size;
  size = FOURBYTE_ALIGN(size);
  return size;
}

/**
 * Caller is responsible for freeing *out.  Will return HPX_ERROR if (1)
 * payload size is not 0 and (2) payload is NULL (a NULL pointer is allowed if
 * payload size is 0)
 */
hpx_error_t serialize(const struct hpx_parcel *p, struct header **out) {
  /* preconditions */
  if (!p)
    return (__hpx_errno = HPX_ERROR);
  
  if (!out)
    return (__hpx_errno = HPX_ERROR);

  if (p->payload_size && !p->payload)
    return (__hpx_errno = HPX_ERROR);
  
  /* allocate space for binary blob */
  struct header *blob = *out;
  size_t size_of_blob = sizeof(*blob) + p->payload_size;
  size_of_blob = FOURBYTE_ALIGN(size_of_blob); /* in case we're using UGNI */
  //blob = hpx_alloc(size_of_blob);
  //  if (blob == NULL)
  int success;
  success = hpx_alloc_align((void**)&blob, 64, size_of_blob);
  if (success != 0 || blob == NULL)
    return (__hpx_errno = HPX_ERROR_NOMEM);
#if 0
  /* need to unpin this again somewhere - right now the parcel handler does that*/
#if DEBUG_HANDLER
	printf("%d: Pinning/allocating %zd bytes at %tx\n", hpx_get_rank(), size_of_blob, (ptrdiff_t)blob);
	fflush(stdout);
#endif
  __hpx_network_ops->pin((void*)blob, size_of_blob);
#endif

  /* copy the parcel struct, and the payload to the blob
     LD: note the first memcpy doesn't copy the payload pointer
  */
  memcpy(blob, p, sizeof(*blob));
  memcpy(&blob->payload, p->payload, p->payload_size);
  *out = blob;
  return HPX_SUCCESS;
}

/**
 * caller is reponsible for free()ing *p and *p->payload
 */
hpx_error_t deserialize(const struct header* blob, struct hpx_parcel** out) {
  /* preconditions */
  if (!blob)
    return (__hpx_errno = HPX_ERROR);
  
  if (!out)
    return (__hpx_errno = HPX_ERROR);

  struct hpx_parcel *p = *out;
  p =  hpx_alloc(sizeof(*p));
  if (p == NULL)
    return (__hpx_errno = HPX_ERROR_NOMEM);
  
  memcpy(p, blob, sizeof(*blob));
  p->payload = (p->payload_size) ? hpx_alloc(p->payload_size) : NULL;
  if (p->payload_size && p->payload == NULL) {
    hpx_free(p);
    return (__hpx_errno = HPX_ERROR_NOMEM);
  }

#if DEBUG_HANDLER
  printf("%d: copying %zu bytes of memory at %#tx from blob at %#tx\n", hpx_get_rank(), (uintptr_t)blob->payload_size, (uintptr_t)&blob->payload, (uintptr_t)blob);
  fflush(stdout);
#endif
  memcpy(p->payload, &blob->payload, blob->payload_size);
  *out = p;
  return HPX_SUCCESS;
}


/* DO NOT FREE THE RETURN VALUE */
/* FIXME CAUTION for some reason using this causes a bug (alisaing
 * issues?). gcc interprets the return value as a 32 bit integer and sign
 * extends it to 64 bits resulting in BAD THINGS happening...
 *
 * LD: this doesn't appear to be used?
 */
/* static hpx_parcel_t* read_header(char* blob) { */
/*   return (hpx_parcel_t*)blob; */
/* } */
