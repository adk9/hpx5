/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Parcel Handler Function Definitions
  hpx_parcelhandler.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of 
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Benjamin D. Martin <benjmart [at] indiana.edu>
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_PARCEL_PARCELHANDLER_H_
#define LIBHPX_PARCEL_PARCELHANDLER_H_

/**
 * Currently, the HPX parcel layer is implemented as a global service thread,
 * the parcel-handler thread.
 */

#include "hpx/system/attributes.h"

/**
 * Forward declare the pointer types used in this header.
 */
struct hpx_context;
struct parcelhandler HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL);

/**
 * Create the parcel handler. Returns a newly allocated and initialized
 * parcel handler. 
 *
 * NB: In the present implementation, there should only ever be one parcel
 *     handler per context, or the system will break.
 *
 * @param[in] ctx - the context to create the handler in
 *
 * @returns a pointer to the parcel-handler structure
 */
struct parcelhandler *parcelhandler_create(struct hpx_context *ctx)
  HPX_ATTRIBUTE(/* HPX_VISIBILITY_INTERNAL, LD: testsuite wants this */
                HPX_RETURNS_NON_NULL);

/**
 * Clean up the parcel handler: Destroy any outstanding threads in a clean way,
 * free any related data structures, and free the parcel handler itself.
 *
 * @param[in] ph - the pointer returned from hpx_parcelhandler_create().
 */
void parcelhandler_destroy(struct parcelhandler *ph)
  HPX_ATTRIBUTE(/* HPX_VISIBILITY_INTERNAL LD: testsuite wants this */);

#endif /* LIBHPX_PARCEL_PARCELHANDLER_H_ */
