/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  User-Level Parcel Definition
  hpx/future.h

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

#ifndef HPX_FUTURE_H_
#define HPX_FUTURE_H_

/**
 * @file
 * @brief Declares the user-visible interface to HPX futures.
 */

#include <stddef.h>                             /* size_t */
#include "platform.h"                           /* MACROS */
#include "types.h"                              /* struct hpx_future */

/**
 *
 */
struct hpx_future *hpx_future_create(void);
void hpx_future_destroy(struct hpx_future *future);

/**
 * Set the future, which will wake up any threads that are waiting for it. Does
 * not set a value on the future.
 *
 * @param[in] f - the future to set
 */
void hpx_future_set(struct hpx_future * const f)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

/**
 * Sets the future with a value, which will wake up any threads that are waiting
 * for it.
 *
 * If @p bytes > 0 then @p value != NULL.
 *
 * @param[in] f     - the future to set
 * @param[in] bytes - the number of bytes pointed to by value
 * @param[in] value - a pointer to the data
 */
void hpx_future_setv(struct hpx_future * const f, size_t bytes,
                     const void * const value)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

#endif /* HPX_FUTURE_H_ */
