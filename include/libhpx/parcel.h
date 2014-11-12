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

/// The hpx_parcel structure is what the user-level interacts with.
///
/// @field       ustack A pointer to a stack.
/// @field          src The src rank for the parcel.
/// @field         size The data size in bytes.
/// @field       action The target action identifier.
/// @field       target The target address for parcel_send().
/// @field     c_action The continuation action identifier.
/// @field     c_target The target address for the continuation.
/// @field       buffer Either an in-place payload, or a pointer.
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

hpx_parcel_t *parcel_create(hpx_addr_t addr, hpx_action_t action,
                            const void *args, size_t len, hpx_addr_t c_target,
                            hpx_action_t c_action, hpx_pid_t pid, bool inplace)
  HPX_INTERNAL;

void parcel_set_stack(hpx_parcel_t *p, struct ustack *stack)
  HPX_NON_NULL(1) HPX_INTERNAL;
struct ustack *parcel_get_stack(hpx_parcel_t *p)
  HPX_NON_NULL(1) HPX_INTERNAL;

void parcel_set_credit(hpx_parcel_t *p, const uint64_t credit)
  HPX_NON_NULL(1) HPX_INTERNAL;

uint64_t parcel_get_credit(hpx_parcel_t *p)
  HPX_NON_NULL(1) HPX_INTERNAL;


/// Treat a parcel as a stack of parcels, and pop the top.
///
/// This uses the stack field of the parcel to chain the parcels together, so it
/// can't be used with parcels that have active stacks.
///
/// @param[in,out] stack The address of the top parcel in the stack, modified
///                      as a side effect of the call.
///
/// @returns            NULL, or the parcel that was on top of the stack.
hpx_parcel_t *parcel_stack_pop(hpx_parcel_t **stack)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Treat a parcel as a stack of parcels, and pop the top using
/// synchronization.
///
/// This uses the stack field of the parcel to chain the parcels together, so it
/// can't be used with parcels that have active stacks.
///
/// @param[in,out] stack The address of the top parcel in the stack, modified
///                      as a side effect of the call.
///
/// @returns            NULL, or the parcel that was on top of the stack.
hpx_parcel_t *parcel_stack_sync_pop(hpx_parcel_t **stack)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Treat a parcel as a stack of parcels, and push the parcel.
///
/// This uses the stack field of the parcel to chain the parcels together, so it
/// can't be used with parcels that have active stacks.
///
/// @param[in,out] stack The address of the top parcel in the stack, modified
///                      as a side effect of the call.
/// @param[in]    parcel The new top of the stack.
void parcel_stack_push(hpx_parcel_t **stack, hpx_parcel_t *parcel)
  HPX_INTERNAL HPX_NON_NULL(1, 2);


/// Treat a parcel as a stack of parcels, and push the parcel using
/// synchronization.
///
/// This uses the stack field of the parcel to chain the parcels together, so it
/// can't be used with parcels that have active stacks.
///
/// @param[in,out] stack The address of the top parcel in the stack, modified
///                      as a side effect of the call.
/// @param[in]    parcel The new top of the stack.
void parcel_stack_sync_push(hpx_parcel_t **stack, hpx_parcel_t *parcel)
  HPX_INTERNAL HPX_NON_NULL(1, 2);


#endif // LIBHPX_PARCEL_H
