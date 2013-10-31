/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  User-Level Parcel Definition
  parcel/parcel.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of 
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
  Luke Dalessandro <ldalessa [at] indiana.edu>
  ====================================================================
*/

#ifndef HPX_PARCEL_PARCEL_H_
#define HPX_PARCEL_PARCEL_H_

#include <stddef.h>                             /* size_t */
#include "hpx2/action.h"                        /* hpx_action_t */
#include "hpx2/platform.h"                      /* HPX_NON_NULL */
#include "address.h"                            /* struct hpx_address */

/**
 * The hpx_parcel structure is what the user-level interacts with.
 *
 * The layout of this structure is both important and subtle. The go out onto
 * the network directly, hopefully without being copied in any way. We make sure
 * that the relevent parts of the parcel (i.e., those that need to be sent) are
 * arranged contiguously.
 *
 * NB: if you change the layout here, make sure that the network system can deal
 *     with it.
 */
struct hpx_parcel {
  struct hpx_parcel        *next;               /**< always in lists */
  void                     *data;               /**< a pointer to the data */
  struct hpx_address     address;               /**< the target of the parcel */
  hpx_action_t            action;               /**< the action to perform */
  uint8_t              payload[];               /**< an in-place payload */
};

/**
 * Get the size of the parcel's data segment.
 *
 * @param[in] parcel - the parcel to query
 * @returns the size of parcel->data
 */
size_t parcel_get_data_size(const struct hpx_parcel * const parcel)
  HPX_ATTRIBUTE(HPX_NON_NULL(1),
                HPX_VISIBILITY_INTERNAL);

void * const parcel_get_send_offset(const struct hpx_parcel * const parcel)
  HPX_ATTRIBUTE(HPX_NON_NULL(1),
                HPX_RETURNS_NON_NULL,
                HPX_VISIBILITY_INTERNAL);

#endif /* HPX_PARCEL_PARCEL_H_ */
