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
#ifndef LIBHPX_PARCEL_H
#define LIBHPX_PARCEL_H

#include "hpx/hpx.h"

/// ----------------------------------------------------------------------------
/// The hpx_parcel structure is what the user-level interacts with.
///
/// @field      sp - a saved stack pointer
/// @field    size - the data size in bytes
/// @field   state - tracks if the data is in-place
/// @field  action - the target action identifier
/// @field  target - the target address for parcel_send()
/// @field    cont - the continuation address
/// @field    data - either an in-place payload, or a pointer
/// ----------------------------------------------------------------------------
struct hpx_parcel {
  void            *sp;
  int            size;
  int             pid;
  hpx_action_t action;
  hpx_addr_t   target;
  hpx_addr_t     cont;
  char         data[];
};


/// ----------------------------------------------------------------------------
/// Initialize a parcel.
///
/// Parcels may be in place or out of place, and may have a larger data capacity
/// than necessary. This call initializes the parcel correctly.
///
/// @param    p - the parcel pointer
/// @param size - the payload size for the parcel
/// ----------------------------------------------------------------------------
HPX_INTERNAL void parcel_init(hpx_parcel_t *p, int size)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Finalize a parcel.
///
/// If the parcel is out of place, this will de-allocate the buffer.
///
/// @param p - the parcel pointer
/// ----------------------------------------------------------------------------
HPX_INTERNAL void parcel_fini(hpx_parcel_t *p)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Perform a pop operation on a list of parcels.
/// ----------------------------------------------------------------------------
HPX_INTERNAL hpx_parcel_t *parcel_pop(hpx_parcel_t **list)
  HPX_NON_NULL(1);


/// ----------------------------------------------------------------------------
/// Perform a push operation on a list of parcels.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void parcel_push(hpx_parcel_t **list, hpx_parcel_t *p)
  HPX_NON_NULL(1, 2);


/// ----------------------------------------------------------------------------
/// Perform a concatenation of two parcel lists.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void parcel_cat(hpx_parcel_t **list, hpx_parcel_t *p)
  HPX_NON_NULL(1, 2);


#endif // LIBHPX_PARCEL_H
