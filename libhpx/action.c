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

#include "libhpx/action.h"
#include "uthash.h"

#ifdef ENABLE_TAU
#define TAU_DEFAULT 1
#include <TAU.h>
#endif

typedef struct {
  void *f;
  const char *key;
  UT_hash_handle hh;
} action_table_t;

#ifdef ENABLE_DEBUG
static action_table_t *action_table = 0;
#endif

bool hpx_action_eq(const hpx_action_t lhs, const hpx_action_t rhs) {
  return (lhs == rhs);
}


hpx_action_t action_register(const char *key, hpx_action_handler_t f) {
#ifdef ENABLE_DEBUG
  action_table_t *e = malloc(sizeof(*e));
  e->f = (void*)(hpx_action_t)f;
  e->key = key;
  HASH_ADD_PTR(action_table, f, e);
#endif
  return (hpx_action_t)f;
}


hpx_action_handler_t action_lookup(hpx_action_t id) {
  return (hpx_action_handler_t)id;
}


int action_invoke(hpx_action_t action, void *args) {
  hpx_action_handler_t handler = action_lookup(action);
  return handler(args);
}


const char *action_get_key(hpx_action_t id) {
  const char *key = NULL;
#ifdef ENABLE_DEBUG
  action_table_t *e;
  HASH_FIND_PTR(action_table, &id, e);
  if (e)
    key = e->key;
#endif
  return key;
}


hpx_action_t
hpx_register_action(const char *id, hpx_action_handler_t func) {
#ifdef ENABLE_TAU
    TAU_PROFILE("hpx_register_action", "", TAU_DEFAULT);
#endif
  return action_register(id, func);
}
