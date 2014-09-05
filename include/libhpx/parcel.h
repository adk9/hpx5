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
#include "libsync/sync.h"

struct ustack;

/// ----------------------------------------------------------------------------
/// The hpx_parcel structure is what the user-level interacts with.
///
/// @field   ustack - a pointer to a stack
/// @field      src - the src rank for the parcel
/// @field     size - the data size in bytes
/// @field   action - the target action identifier
/// @field   target - the target address for parcel_send()
/// @field c_action - the continuation action identifier
/// @field c_target - the target address for the continuation 
/// @field   buffer - either an in-place payload, or a pointer
/// ----------------------------------------------------------------------------
struct hpx_parcel {
  struct ustack *ustack;
  int               src;
  uint32_t         size;
  hpx_action_t   action;
  hpx_addr_t     target;
  hpx_action_t c_action;
  hpx_addr_t   c_target;
  hpx_pid_t         pid;
  uint64_t       credit;
  char         buffer[];
};

HPX_INTERNAL hpx_parcel_t *
parcel_create(hpx_addr_t addr, hpx_action_t action, const void *args,
              size_t len, hpx_addr_t c_target, hpx_action_t c_action,
              hpx_pid_t pid, bool inplace);

HPX_INTERNAL void parcel_set_stack(hpx_parcel_t *p, struct ustack *stack)
  HPX_NON_NULL(1);
HPX_INTERNAL struct ustack *parcel_get_stack(hpx_parcel_t *p)
  HPX_NON_NULL(1);

HPX_INTERNAL void parcel_set_credit(hpx_parcel_t *p, const uint64_t credit)
  HPX_NON_NULL(1);
HPX_INTERNAL uint64_t parcel_get_credit(hpx_parcel_t *p)
  HPX_NON_NULL(1);

#endif // LIBHPX_PARCEL_H
