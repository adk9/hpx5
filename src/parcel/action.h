/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#ifndef LIBHPX_PARCEL_ACTION_H_
#define LIBHPX_PARCEL_ACTION_H_

#include "hpx/action.h"                         /* hpx_action_t */
#include "hpx/system/attributes.h"

/**
 * Find the local mapping for an action.
 *
 * @param[in] key - the key we're looking up
 * @returns the function we found
 */
hpx_func_t action_lookup(hpx_action_t key)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL);

#endif /* LIBHPX_PARCEL_ACTION_H_ */
