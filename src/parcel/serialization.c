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

#include <errno.h>                              /* ENOMEM/EINVAL */
#include <stddef.h>                             /* size_t */
#include <stdint.h>                             /* uint8_t */
#include <string.h>                             /* memcpy */

#include "serialization.h"
#include "hpx/globals.h"                        /* __hpx_network_ops */
#include "hpx/mem.h"                            /* hpx_{alloc,free} */
#include "hpx/parcel.h"                         /* hpx_parcel_acquire */
#include "parcel.h"                             /* struct parcel */
#include "debug.h"                              /* dbg_printf */
#include "network.h"                            /* struct network_mgr */

/** Typedefs used for convenience in this source file @{ */
typedef struct header     header_t;
typedef struct hpx_parcel parcel_t;
/** @} */

static size_t
header_size(size_t bytes)
{
  size_t size = sizeof(header_t) + bytes;
  return __hpx_network_ops->get_network_bytes(size);
}

static header_t *
header_alloc(size_t bytes)
{
  header_t* header = NULL;
  size_t size      = header_size(bytes);
  int success      = hpx_alloc_align((void**)&header, HPX_CACHELINE_SIZE, size);
  if (success == ENOMEM) {
    __hpx_errno = HPX_ERROR_NOMEM;
    return NULL;
  }
  
  if (success == EINVAL) {
    __hpx_errno = HPX_ERROR;
    return NULL;
  }

  header->size = size;  /* need to remember the actual size of the header */
  __hpx_network_ops->pin(header, size);
  return header;
}

static header_t *
header_init(header_t *header, const parcel_t *parcel)
{
  header->parcel_id    = 0;
  header->action       = parcel->action;
  header->target       = parcel->target;
  header->cont         = parcel->cont;
  header->flags        = 0;
  header->payload_size = parcel_get_data_size(parcel);
  memcpy(&header->payload, parcel->data, parcel_get_data_size(parcel));
  return header;
}

/**
 * Caller is responsible for freeing *out. Will return NULL if (1) payload size
 * is not 0 and (2) payload is NULL (a NULL pointer is allowed if payload size
 * is 0)
 */
header_t *
serialize(const parcel_t *parcel)
{
  /* preconditions */
  if (!parcel)
    return NULL;

  size_t bytes = parcel_get_data_size(parcel);
  if (bytes && !parcel->data)
    return NULL;

  /* allocate a "big enough" header and initialize it */
  header_t *out = header_alloc(bytes);
  if (!out)
    return NULL;
  
  return header_init(out, parcel);
}

/**
 * caller is reponsible for free()ing *p and *p->payload
 */
parcel_t *
deserialize(const header_t *header)
{
  dbg_assert_precondition(header);

  parcel_t *out = hpx_parcel_acquire(header->payload_size);
  if (!out)
    dbg_print_error(__hpx_errno, "Could not deserialize a network header.");

  out->action = header->action;
  out->target = header->target;
  out->cont   = header->cont;
  dbg_printf("%d: copying %zu bytes from %p to parcel at %p\n",
             hpx_get_rank(), header->payload_size, (void*)&header->payload,
             (void*)out);
  memcpy(out->data, &header->payload, header->payload_size);
  return out;
}
