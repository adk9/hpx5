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
#ifndef LIBHPX_ACTION_H
#define LIBHPX_ACTION_H

#include <hpx/hpx.h>


///
struct action_table;


/// Get the key for an action.
const char *action_table_get_key(const struct action_table *, hpx_action_t)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Get the handler associated with an action.
hpx_action_handler_t action_table_get_handler(const struct action_table *,
                                              hpx_action_t)
  HPX_INTERNAL HPX_NON_NULL(1);


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


#define LIBHPX_REGISTER_ACTION        HPX_REGISTER_ACTION
#define LIBHPX_REGISTER_PINNED_ACTION HPX_REGISTER_PINNED_ACTION
#define LIBHPX_REGISTER_TASK          HPX_REGISTER_TASK
#define LIBHPX_REGISTER_INTERRUPT     HPX_REGISTER_INTERRUPT

#endif // LIBHPX_ACTION_H
