/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Parcel Handler Function Definitions
  libhpx/parcelhandler.h

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

#ifndef LIBHPX_PARCEL_PARCELHANDLER_H_
#define LIBHPX_PARCEL_PARCELHANDLER_H_

/**
 * Currently, the HPX parcel layer is implemented as a global service thread,
 * the parcel-handler thread.
 */

#include "hpx/system/attributes.h"

/** Forward declarations @{ */
struct hpx_context;
struct hpx_locality;
struct hpx_future;
struct hpx_parcel;
struct HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL) parcelhandler;
/** @} */

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


/**
 * Make a parcel handler request to send a parcel.
 *
 * This should only be called to put a parcel onto the network, i.e., it does
 * not do a special check to see if the target address is local. Loopback should
 * work fine, but is less efficient than just spawning a thread locally.
 *
 * @param[in]     dest - destination locality (HACK for virtual address)
 * @param[in]   parcel - the parcel to send
 * @param[in] complete - a 0-size future to trigger when send completes locally
 *                       (optional)
 * @param[in]   thread - a sizeof(struct hpx_thread *)-size future to trigger
 *                       when a thread has been generated to handle the action
 *                       (optional)
 * @param[out]  result - a future for the result
 *
 * @returns HPX_SUCCESS or an error code (@see error.h).
 */
int parcelhandler_send(struct hpx_locality *dest,
                       const struct hpx_parcel *parcel,
                       struct hpx_future *complete,
                       struct hpx_future *thread,
                       struct hpx_future **result)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL,
                HPX_NON_NULL(1, 2));

#endif /* LIBHPX_PARCEL_PARCELHANDLER_H_ */
