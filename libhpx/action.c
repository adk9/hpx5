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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/utils.h"

int LIBHPX_NUM_ACTIONS;
hpx_action_table _action_table = 0;


const char *action_get_key(hpx_action_t id) {
  return (const char*)(_action_table[id].key);
}


int _cmp_keys(const void *a, const void *b) {
  const _action_entry *ea = (const _action_entry*)a;
  const _action_entry *eb = (const _action_entry*)b;
  return strcmp(ea->key, eb->key);
}


int hpx_finalize_actions(void) {
  qsort(_action_table, LIBHPX_NUM_ACTIONS, sizeof(*_action_table), _cmp_keys);
  for (int i = 0; i < LIBHPX_NUM_ACTIONS; ++i)
    *(_action_table[i].action) = i;
  return HPX_SUCCESS;
}


hpx_action_table libhpx_initialize_actions(void) {
  if (!_action_table)
    _action_table = malloc(sizeof(*_action_table) * LIBHPX_ACTION_TABLE_SIZE);
  return _action_table;
}


int hpx_register_action(hpx_action_t *action, const char *key, hpx_action_handler_t func) {
  if (!_action_table)
    libhpx_initialize_actions();
  _action_entry *entry = &(_action_table[LIBHPX_NUM_ACTIONS++]);
  entry->func = func;
  entry->action = action;
  entry->key = key;
  return HPX_SUCCESS;
}

