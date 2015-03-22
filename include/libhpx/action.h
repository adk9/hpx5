// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef LIBHPX_ACTION_H
#define LIBHPX_ACTION_H

#include <stdarg.h>
#include <hpx/hpx.h>

///
struct action_table;
struct hpx_parcel;


/// Get the key for an action.
const char *action_table_get_key(const struct action_table *, hpx_action_t)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Get the action type.
hpx_action_type_t action_table_get_type(const struct action_table *, hpx_action_t)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Get the key for an action.
hpx_action_handler_t action_table_get_handler(const struct action_table *, hpx_action_t)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Run the handler associated with an action.
int action_execute(struct hpx_parcel *)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Get the FFI type information associated with an action.
ffi_cif *action_table_get_cif(const struct action_table *, hpx_action_t)
  HPX_INTERNAL HPX_NON_NULL(1);

/// Serialize the vargs into the parcel.
hpx_parcel_t *action_pack_args(hpx_parcel_t *p, int n, va_list *vargs)
  HPX_INTERNAL;

/// Returns a parcel that encodes the target address, an action and
/// its argument, and the continuation. The parcel is ready to be sent
/// to effect a call operation.
hpx_parcel_t *action_parcel_create(hpx_addr_t addr, hpx_action_t action,
                                   hpx_addr_t c_addr, hpx_action_t c_action,
                                   int nargs, va_list *args)
  HPX_INTERNAL;

/// Call an action by sending a parcel given a list of variable args.
int libhpx_call_action(hpx_addr_t addr, hpx_action_t action, hpx_addr_t c_addr,
                       hpx_action_t c_action, hpx_addr_t lsync, hpx_addr_t gate,
                       int nargs, va_list *args)
  HPX_INTERNAL;

/// Is the action a pinned action?
bool action_is_pinned(const struct action_table *, hpx_action_t)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Is the action a task?
bool action_is_task(const struct action_table *, hpx_action_t)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Is the action an interrupt?
bool action_is_interrupt(const struct action_table *, hpx_action_t)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Build an action table.
///
/// This will process all of the registered actions, sorting them by key and
/// assigning ids to their registered id addresses. The caller obtains ownership
/// of the table and must call action_table_free() to release its resources.
///
/// @return             An action table that can be indexed by the keys
///                     originally registered.
const struct action_table *action_table_finalize(void)
  HPX_INTERNAL;


/// Free an action table.
void action_table_free(const struct action_table *action)
  HPX_INTERNAL HPX_NON_NULL(1);


#endif // LIBHPX_ACTION_H
