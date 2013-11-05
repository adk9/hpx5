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
#include "system/attributes.h"                  /* HPX MACROS */

/** Forward declarations @{ */
struct hpx_future;
/** @} */

/**
 * Allocate a future.
 *
 * @param[in] bytes - the number of bytes we need for the future's value
 *
 * @returns the future, or NULL if there was an error
 */
struct hpx_future *hpx_future_create(size_t bytes);

/**
 * Free a future.
 *
 * @param[in] future - the future to free, may be NULLo
 */
void hpx_future_destroy(struct hpx_future *future);

/**
 * Set the future, which will wake up any threads that are waiting for it. Does
 * not set a value on the future.
 *
 * @param[in] future - the future to set
 */
void hpx_future_set(struct hpx_future *future)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

/**
 * Sets the future with a value, which will wake up any threads that are waiting
 * for it.
 *
 * If @p bytes > 0 then @p value != NULL.
 *
 * @param[in] future - the future to set
 * @param[in]  bytes - the number of bytes pointed to by value
 * @param[in]  value - a pointer to the data
 */
void hpx_future_setv(struct hpx_future *future, size_t bytes, const void *value)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

#endif /* HPX_FUTURE_H_ */
