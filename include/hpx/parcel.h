/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  User-Level Parcel Definition
  hpx/parcel.h

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

#ifndef HPX_PARCEL_H_
#define HPX_PARCEL_H_

/**
 * @file @brief Provides the user-level parcel interface.
 */
#include "hpx/system/attributes.h"              /* MACROS */
#include "hpx/action.h"                         /* hpx_action_t */

/** typedefs provided by this header
    @{ */
typedef struct hpx_parcel hpx_parcel_t;
/** @} */

/** forward declarations
    @{ */
struct hpx_future;
struct hpx_address;
/** @} */

/**
 * Acquire a user-level parcel from the runtime.
 *
 * This function provides a scheduling opportunity to the runtime, i.e., the
 * calling thread may be blocked and context switch during its execution. It is
 * valid to assume that this will only occur during parcel-resource
 * contention. It is guaranteed to be lock-free.
 *
 * The parcel @b must be released back to the runtime using
 * hpx_parcel_release(), or it may be leaked.
 *
 * @param[in] bytes - the number of bytes in the payload to allocate
 * @returns a pointer to an allocated hpx_parcel, or NULL if there is an error
 */
hpx_parcel_t *hpx_parcel_acquire(int bytes);

/**
 * Release a user-level parcel back to the runtime.
 *
 * This function does not provide a scheduling opportunity to the runtime,
 * i.e., the calling thread will not be context switched during its execution. 
 *
 * It is not an error to release a NULL pointer.
 *
 * It *is* an error to:
 *
 *   -# release any non-NULL address that was not returned from
 *      hpx_parcel_acquire (@see hpx_parcel_acquire).
 *   -# release a parcel that was acquired by a different user-level thread
 *   -# release a parcel twice
 *
 * These conditions will not be checked, and produce undefined results.
 *
 * @param[in] parcel the parcel to release
 */
void hpx_parcel_release(hpx_parcel_t *parcel);

/**
 * Create a clone of a parcel.
 *
 * This call has the same scheduling and progress semantics as
 * hpx_parcel_acquire(). The clone is a "deep clone" of @p parcel at the time
 * of the call.
 *
 * The returned clone @b must be released back to the runtime usign
 * hpx_parcel_release(), or it may be leaked.
 *
 * @param[in] parcel - the parcel to clone
 * @returns the clone, or NULL if there was an error during cloning
 */
hpx_parcel_t *hpx_parcel_clone(hpx_parcel_t *parcel)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

/**
 * Copy the data from one parcel into another.
 *
 * This call performs a "deep copy" of @p from. It does not provide a
 * scheduling opportunity to the runtime, i.e., the calling thread will not be
 * context switched during its execution. It is not an error to copy data to a
 * smaller parcel, the runtime will not attempt to copy more data out of @p
 * from than @p to can receive.
 *
 * @pre @p to must have been allocated with at least as much payload data as @p
 * from.
 *
 * @param[in] to   - the target parcel
 * @param[in] from - the source parcel
 * @returns @p to, or NULL if there was an error
 */
hpx_parcel_t *hpx_parcel_copy(hpx_parcel_t * restrict to,
                                   const hpx_parcel_t * restrict from)
  HPX_ATTRIBUTE(HPX_NON_NULL(1, 2), HPX_RETURNS_NON_NULL);

/**
 * Initiate a parcel send operation. Note that Calling this function transfers                                                                                                                                                   
 * ownership of the parcel to the runtime system which will release it.
 *
 * This operation does not provide a scheduling opportunity to the runtime,
 * i.e., the calling thread will not be context switched during its execution.
 *
 * @param[in]     dest - the destination locality; may be freed immediately upon 
 *                       return (HACK)
 * @param[in]   parcel - the parcel to send, must be non-NULL
 * @param[in]   thread - a future that will be triggered with the address of the
 *                       remot thread when the send is complete, may be NULL
 * @param[in]   result - the address of a future that will be triggered when the
 *                       thread spawned  as a result of this future returns, may
 *                       be NULL 
 * @returns an error indicating a problem with the runtime
 */
int hpx_parcel_send(struct hpx_locality *dest,
                    hpx_parcel_t *parcel,
                    struct hpx_future *thread,
                    struct hpx_future **result)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

/**
 * Resize a parcel data block.
 *
 * This operation is semantically equivalent to the following code fragment.
 *
 * @code
 * hpx_parcel_t *parcel = {...};
 * int size = {...};
 * hpx_parcel_t *q = hpx_parcel_acquire(size);
 * hpx_parcel_copy(q, parcel);
 * hpx_parcel_release(parcel);
 * parcel = q;
 * @endcode
 *
 * In particular, it provides an opportunity for scheduling and has a progress
 * guarantee that is the weakest of those of hpx_parcel_acquire(),
 * hpx_parcel_copy(), and hpx_parcel_release(). In addition, the original
 * @p parcel may be released as a side effect of this operation---application
 * developers must be careful not to retain aliases to @p across the call.
 *
 * Note that it provides no benefit to resize a parcel to a smaller size.
 *
 * @param[in] parcel - the parcel to resize
 * @param[in] size   - the target size
 * @returns non-zero if an error condition occurred
 */
int hpx_parcel_resize(hpx_parcel_t **parcel, int size)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

/**
 * Get the target address for a parcel.
 *
 * This does not provide a scheduling opportunity, and does not have any side
 * effects.
 *
 * @param[in] parcel - the parcel to query
 *
 * @returns the current target address
 */
struct hpx_addr hpx_parcel_get_target(const hpx_parcel_t *parcel)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

/**
 * Set the target address for a parcel.
 *
 * This does not provide a scheduling opportunity, and does not have any side
 * effects.
 *
 * @param[in] parcel - the parcel to update
 * @param[in]   addr - the new target address
 */
void hpx_parcel_set_target(hpx_parcel_t *parcel, struct hpx_addr addr)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

/**
 * Get the continuation address for a parcel.
 *
 * This does not provide a scheduling opportunity, and does not have any side
 * effects.
 *
 * @param[in] parcel - the parcel to query
 *
 * @returns the current continuation address
 */
struct hpx_addr hpx_parcel_get_cont(const hpx_parcel_t *parcel)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

/**
 * Set the continuation address for a parcel.
 *
 * This does not provide a scheduling opportunity, and does not have any side
 * effects.
 *
 * @param[in] parcel - the parcel to update
 * @param[in]   addr - the new continuation address
 */
void hpx_parcel_set_cont(hpx_parcel_t *parcel, struct hpx_addr addr)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

/**
 * Get a copy of the current action for a parcel.
 *
 * This does not provide a scheduling opportunity. The action can be set using
 * hpx_parcel_set_action(). This operation does not have any side effects.
 *
 * @param[in] parcel - the parcel to query
 * @returns the current action
 */
hpx_action_t hpx_parcel_get_action(const hpx_parcel_t *parcel)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

/**
 * Set the current action for a parcel.
 *
 * This does not provide a scheduling opportunity. The action can be read using
 * hpx_parcel_get_action(). This operation has no other side effects.
 *
 * @param[in] parcel - the parcel to query
 * @param[in] action - the new action
 */
void hpx_parcel_set_action(hpx_parcel_t *parcel, hpx_action_t action)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

/**
 * Get a parcel's data payload block.
 *
 * This does not provide a scheduling opportunity, and does not have any side
 * effects. The returned value can be used to update the data payload. The user
 * is responsible for restricting access to the payload based on the size the
 * parcel was allocated with. The pointer will continue to point at the source
 * parcel after an hpx_parcel_copy() or hpx_parcel_clone(). The pointer should
 * not be retained across a call to hpx_parcel_resize(), as the address of the
 * payload can change.
 *
 * @param[in] parcel - the parcel to query
 * @returns a pointer to the payload
 */
void *hpx_parcel_get_data(hpx_parcel_t *parcel)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

/**
 * Set a parcel's data payload block.
 *
 * @param[in] parcel - the parcel to query
 * @param[in]   data - the address of the data to set
 * @param[in] length - the number of bytes of data to set
 */
void hpx_parcel_set_data(hpx_parcel_t * restrict parcel, void * restrict data, int length)
  HPX_ATTRIBUTE(HPX_NON_NULL(1,2));

#endif /* HPX_PARCEL_H_ */
