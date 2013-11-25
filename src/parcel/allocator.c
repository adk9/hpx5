/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  ParcelQueue Functions
  src/parcel/allocator.c

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
#include <stdlib.h>                             /* malloc/free */
#include <strings.h>                            /* bzero */

#include "hpx/parcel.h"                         /* hpx_parcel_t */
#include "allocator.h"
#include "debug.h"
#include "parcel.h"                             /* struct hpx_parcel */

hpx_parcel_t *
parcel_get(size_t bytes)
{
  hpx_parcel_t *p = malloc(sizeof(*p) + bytes);
  p->data         = &p->payload;
  p->size         = bytes;
  p->action       = HPX_ACTION_NULL;
  p->target       = HPX_NULL;
  p->cont         = HPX_NULL;
  bzero(&p->payload, bytes);
  return p;
}

void
parcel_put(hpx_parcel_t *parcel)
{
  dbg_assert_precondition(parcel);
  free(parcel);
}
