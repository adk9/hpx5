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
  Luke Dalessandro   <ldalessa [at] indiana.edu>
  ====================================================================
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stddef.h>                             /* size_t */

/** typedefs used locally in this source file @{ */
typedef struct hpx_future future_t;
/** @} */

future_t *
hpx_future_create(size_t bytes)
{
  return NULL;
}

void
hpx_future_destroy(future_t *future)
{
}

void
hpx_future_set(struct hpx_future *future)
{
}

void
hpx_future_setv(future_t *future, size_t bytes, const void *value)
{
}
