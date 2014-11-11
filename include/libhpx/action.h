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

#include "hpx/hpx.h"

/// The default libhpx action table size.
#define LIBHPX_ACTION_TABLE_SIZE 4096

/// The number of registered actions in the action table.
extern int LIBHPX_NUM_ACTIONS;

/// An HPX action table entry
typedef struct {
  hpx_action_handler_t func;
  hpx_action_t        *action;
  const char          *key;
} _action_entry;

typedef _action_entry *hpx_action_table;

extern hpx_action_table _action_table;

const char *action_get_key(hpx_action_t id)
  HPX_INTERNAL;

hpx_action_table libhpx_initialize_actions(void)
  HPX_INTERNAL;

/// A convenience macro for registering an HPX action
#define LIBHPX_REGISTER_ACTION(act, f)                                 \
  hpx_register_action(act, HPX_STR(_libhpx##f), (hpx_action_handler_t)f)


#endif // LIBHPX_ACTION_H
