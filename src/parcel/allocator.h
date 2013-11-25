/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  ParcelQueue Functions
  src/parcel/allocator.h

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

#ifndef LIBHPX_PARCEL_ALLOCATOR_H_
#define LIBHPX_PARCEL_ALLOCATOR_H_

#include <stddef.h>
#include "hpx/system/attributes.h"

struct hpx_parcel;

/**
 * Get a parcel from the allocator.
 *
 * @param[in] bytes - size of the data segment required
 * @returns A pointer to a parcel, or NULL if there is an error. The parcel will
 *          have its fields initialized to their default value.
 */
struct hpx_parcel *parcel_get(size_t bytes);

/**
 * Return a parcel to the allocator.
 *
 * @param[in] parcel - the parcel to return, must be an address received via
 *                     parcel_get, and must not be NULL
 */
void parcel_put(struct hpx_parcel *parcel)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

#endif /* LIBHPX_PARCEL_ALLOCATOR_H_ */
