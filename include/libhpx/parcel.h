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

struct ustack;

/// ----------------------------------------------------------------------------
/// The hpx_parcel structure is what the user-level interacts with.
///
/// @field   stack - a pointer to a stack
/// @field     src - the src rank for the parcel
/// @field    size - the data size in bytes
/// @field  action - the target action identifier
/// @field  target - the target address for parcel_send()
/// @field    cont - the continuation address
/// @field    data - either an in-place payload, or a pointer
/// ----------------------------------------------------------------------------
struct hpx_parcel {
  struct ustack *stack;
  int              src;
  uint32_t        size;
  hpx_action_t  action;
  hpx_addr_t    target;
  hpx_addr_t      cont;
  char          data[];
};

#endif // LIBHPX_PARCEL_H
