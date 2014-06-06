// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef HPX_PARCEL_H
#define HPX_PARCEL_H

#include "hpx/action.h"
#include "hpx/addr.h"

// ----------------------------------------------------------------------------
/// HPX parcel interface.
///
/// Parcels are the HPX message type.
// ----------------------------------------------------------------------------
typedef struct hpx_parcel hpx_parcel_t;

// ----------------------------------------------------------------------------
/// Acquire a parcel.
///
/// The application programmer can choose to associate an existing buffer with
/// the parcel, or can request that the parcel allocate its own buffer. Parcels
/// are runtime resources and must be released, either explicitly using
/// hpx_parcel_release() or implicitly through hpx_parcel_send() or
/// hpx_parcel_send_sync().
///
/// If the @p data pointer is NULL, then the runtime will allocate a @p bytes
/// sized buffer with the parcel that can be accessed using the
/// hpx_parcel_{get,set}_data() interface. The parcel owns this buffer, and it
/// will be deallocated when the local send completes. The benefit to this mode
/// of operation is that the runtime has the opportunity to allocate an in-line
/// buffer for the data---which can improve performance, particularly for small
/// buffers---and often results in low-latency local completion. The
/// disadvantage of this approach is that the application programmer must
/// explicitly copy data into the parcel, which incurs serial overhead that may
/// be avoided in some cases. In this mode, there is no semantic difference
/// between hpx_parcel_send() and hpx_parcel_send_sync().
///
/// If the @p data pointer is NULL, and the @p bytes is 0, then
/// hpx_parcel_{get,set}_data() may fail.
///
/// If the @p data pointer is non-NULL, then the runtime assumes that @p bytes
/// is an accurate size of @p data, and will use it as the buffer to be
/// sent. The runtime does not claim ownership of @p data, but does make it
/// available through the standard hpx_parcel_{get,set}_data() interface. The @p
/// data buffer MAY be concurrently associated with more than one parcel,
/// however the @p data buffer MUST NOT be written to while any of the parcels
/// that it is associated with has an outstanding hpx_parcel_send()
/// operation, or concurrently with an hpx_parcel_send_sync().
///
/// @param  data a possibly NULL buffer to associate with the parcel
/// @param bytes size of the @p data buffer
/// @returns     a pointer to the parcel structure, or NULL on error
// ----------------------------------------------------------------------------
hpx_parcel_t *hpx_parcel_acquire(void *data, size_t bytes)
  HPX_MALLOC;

// ----------------------------------------------------------------------------
/// Explicitly release a parcel.
///
/// The @p p argument must correspond to a parcel pointer returned from
/// hpx_parcel_acquire().
///
/// @param p the parcel to release
// ----------------------------------------------------------------------------
void hpx_parcel_release(hpx_parcel_t *p)
  HPX_NON_NULL(1);


// ----------------------------------------------------------------------------
/// Send a parcel with asynchronous local completion semantics.
///
/// hpx_parcel_send() has asynchronous local semantics. After returning from
/// this function, the caller must test the @p done future if it cares about
/// local completion. The @p done future may be HPX_NULL if such a test is not
/// performed---this may result in better performance.
///
/// Sending a parcel transfers ownership of the parcel to the runtime. The
/// parcel pointed to by @p p may not be reused and must not be
/// hpx_parcel_release()d.
///
/// @param    p the parcel to send, must correspond to a parcel returned from
///             hpx_parcel_acquire()
/// @param done the address of an LCO to set once the send has completed
///             locally, or HPX_NULL if the caller does not care
// ----------------------------------------------------------------------------
void hpx_parcel_send(hpx_parcel_t *p, hpx_addr_t done)
  HPX_NON_NULL(1);


// ----------------------------------------------------------------------------
/// Send a parcel with synchronous local completion semantics.
///
/// hpx_parcel_send_sync() performs a synchronous local send. After returning
/// from this function, the caller is guaranteed that the local send has
/// completed.
///
/// Sending a parcel transfers ownership of the parcel to the runtime. The
/// parcel pointed to by @p p may not be reused and must not be
/// hpx_parcel_release()d.
///
/// @param p the parcel to send, must correspond to a parcel returned from
///          hpx_parcel_acquire().
// ----------------------------------------------------------------------------
void hpx_parcel_send_sync(hpx_parcel_t *p)
  HPX_NON_NULL(1);


// ----------------------------------------------------------------------------
/// Get the action associated with a parcel
/// @param p the parcel
/// @returns the action associated with @p p
// ----------------------------------------------------------------------------
hpx_action_t hpx_parcel_get_action(const hpx_parcel_t *p)
  HPX_NON_NULL(1);

// ----------------------------------------------------------------------------
/// Get the target address of a parcel
/// @param p the parcel
/// @returns the global address of the target of @p p
// ----------------------------------------------------------------------------
hpx_addr_t hpx_parcel_get_target(const hpx_parcel_t *p)
  HPX_NON_NULL(1);

// ----------------------------------------------------------------------------
/// Get the continuation associated with a parcel
/// @param p the parcel
/// @returns the global address of the continuation of @p p
// ----------------------------------------------------------------------------
hpx_addr_t hpx_parcel_get_cont(const hpx_parcel_t *p)
  HPX_NON_NULL(1);

// ----------------------------------------------------------------------------
/// Get the data pointer for a parcel
/// The data for a parcel can be written to directly, which in some cases
/// may allow one to avoid an extra copy.
/// @param p the parcel
/// @returns a pointer to the data for the parcel
// ----------------------------------------------------------------------------
void *hpx_parcel_get_data(hpx_parcel_t *p)
  HPX_NON_NULL(1);

// ----------------------------------------------------------------------------
/// Set the action for a parcel
/// @param      p the parcel
/// @param action the action to be invoked when the parcel arrives at its target
// ----------------------------------------------------------------------------
void hpx_parcel_set_action(hpx_parcel_t *p, const hpx_action_t action)
  HPX_NON_NULL(1);


// ----------------------------------------------------------------------------
/// Set a target to a parcel, for it to be sent to
/// @param    p the parcel
/// @param addr the address of the target to send the parcel to
// ----------------------------------------------------------------------------
void hpx_parcel_set_target(hpx_parcel_t *p, const hpx_addr_t addr)
  HPX_NON_NULL(1);

// ----------------------------------------------------------------------------
/// Set the continuation for a parcel
/// @param   p the parcel
/// @param lco the address of the continuation
// ----------------------------------------------------------------------------
void hpx_parcel_set_cont(hpx_parcel_t *p, const hpx_addr_t lco)
  HPX_NON_NULL(1);

// ----------------------------------------------------------------------------
/// Set the data for a parcel
/// The data will be copied (shallowly) into the parcel. When possible, it is
/// preferable to use hpx_parcel_get_data() and write to the parcel directly,
/// but hpx_parcel_set_data() may be used when a copy is unavoidable anyway.
/// @param    p the parcel
/// @param data the data to put in the parcel
/// @param size the size of @p data
// ----------------------------------------------------------------------------
void hpx_parcel_set_data(hpx_parcel_t *p, const void *data, int size)
  HPX_NON_NULL(1);

#endif
