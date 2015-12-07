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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libhpx/action.h>
#include <libhpx/debug.h>
#include "table.h"

#define _ACTION_TABLE_GET(type, name, init)                             \
  type action_table_get_##name(const struct action_table *table,        \
                               hpx_action_t id) {                       \
    if (id == HPX_ACTION_INVALID) {                                     \
      log_dflt("action registration is not complete");                  \
      return (type)init;                                                \
    } else if (id >= table->n) {                                        \
      dbg_error("action id, %d, out of bounds [0,%u)\n", id, table->n); \
    }                                                                   \
    return table->entries[id].name;                                     \
  }                                                                     \
  type action_table_get_##name(const struct action_table *table,        \
                               hpx_action_t id)

_ACTION_TABLE_GET(const char *, key, NULL);
_ACTION_TABLE_GET(hpx_action_type_t, type, HPX_ACTION_INVALID);
_ACTION_TABLE_GET(uint32_t, attr, 0);
_ACTION_TABLE_GET(hpx_action_handler_t, handler, NULL);
_ACTION_TABLE_GET(ffi_cif *, cif, NULL);
_ACTION_TABLE_GET(void *, env, NULL);

int action_table_size(const action_table_t *table) {
  return table->n;
}

#ifdef ENABLE_DEBUG
void CHECK_BOUND(const action_table_t *table, hpx_action_t id) {
  if (id == HPX_ACTION_INVALID) {
    dbg_error("action registration is not complete");
  }
  else if (id >= table->n) {
    dbg_error("action id, %d, out of bounds [0,%u)\n", id, table->n);
  }
}
#endif
