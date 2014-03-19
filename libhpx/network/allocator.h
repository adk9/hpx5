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
#ifndef LIBHPX_NETWORK_PARCEL_ALLOCATOR_H
#define LIBHPX_NETWORK_PARCEL_ALLOCATOR_H

#include "hpx/hpx.h"

struct transport;
typedef struct allocator allocator_t;

/// ----------------------------------------------------------------------------
/// Allocate and initialize a parcel allocator.
/// ----------------------------------------------------------------------------
HPX_INTERNAL allocator_t *parcel_allocator_new(struct transport *transport);

/// ----------------------------------------------------------------------------
/// Delete a parcel allocator.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void parcel_allocator_delete(allocator_t *allocator)
  HPX_NON_NULL(1);

/// ----------------------------------------------------------------------------
/// Get a parcel from the allocator.
///
/// @param bytes - size of the data segment required
/// @returns       A pointer to a parcel, or NULL if there is an error. The
///                parcel will have its fields initialized to their default
///                value.
/// ----------------------------------------------------------------------------
HPX_INTERNAL hpx_parcel_t *parcel_allocator_get(allocator_t *, int bytes)
  HPX_NON_NULL(1) HPX_MALLOC;

/// ----------------------------------------------------------------------------
/// Return a parcel to the allocator.
///
/// @param parcel - the parcel to return, must be an address received via
///                 parcel_allocator_get(), and must not be NULL
/// ----------------------------------------------------------------------------
HPX_INTERNAL void parcel_allocator_put(allocator_t *, hpx_parcel_t *parcel)
  HPX_NON_NULL(1, 2);

#endif // LIBHPX_NETWORK_PARCEL_ALLOCATOR_H
