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
# include "config.h"
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "hpx/types.h"
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/utils.h"


/// The default libhpx action table size.
#define LIBHPX_ACTION_TABLE_SIZE 4096

/// Action types
typedef enum {
  _ACTION_UNKNOWN = -1,
  _ACTION_DEFAULT = 0,
  _ACTION_PINNED,
  _ACTION_TASK,
  _ACTION_INTERRUPT,
} _action_type_t;

/// An action table entry type.
///
/// This will store the function and unique key associated with the action, as
/// well as the address of the id the user would like to be assigned during
/// action finalization.
///
/// @note In the future this entry type can contain more information about the
///       actions, e.g., can they block, should we pre-pin their arguments,
///       etc.
typedef struct {
  hpx_action_handler_t func;
  hpx_action_t        *id;
  const char          *key;
  _action_type_t      type;
  ffi_cif             *cif;
} _entry_t;


/// Compare two entries by their keys.
///
/// This is used to sort the action table during finalization, so that we can
/// uniformly assign ids to actions independent of which region in the local
/// address space the functions are loaded into.
///
/// @param          lhs A pointer to the left-hand entry.
/// @param          rhs A pointer tot he right-hand entry.
///
/// @return             The lexicographic comparison of the entries' keys.
static int _cmp_keys(const void *lhs, const void *rhs) {
  const _entry_t *el = lhs;
  const _entry_t *er = rhs;
  return strcmp(el->key, er->key);
}


/// An action table is simply an array that stores its size.
///
///
typedef struct action_table {
  int n;
  _entry_t entries[];
} _table_t;


/// A static action table.
///
/// We currently need to be able to register actions before we call hpx_init()
/// because we use constructors inside of libhpx to do action registration. We
/// expose this action table to be used for that purpose.
static _table_t *_actions = NULL;


/// Get the static action table.
///
/// This is not synchronized and thus unsafe to call in a multithreaded
/// environment, but we make sure to call it in hpx_init() where we assume we
/// are running in single-threaded mode, so we should be safe.
static _table_t *_get_actions(void) {
  if (!_actions) {
    static const int capacity = LIBHPX_ACTION_TABLE_SIZE;
    _actions = malloc(sizeof(*_actions) + capacity * sizeof(_entry_t));
    _actions->n = 0;
    memset(&_actions->entries, 0, capacity * sizeof(_entry_t));
  }

  return _actions;
}


/// Sort the actions in an action table by their key.
static void _sort_entries(_table_t *table) {
  qsort(&table->entries, table->n, sizeof(_entry_t), _cmp_keys);
}


/// Assign all of the entry ids in the table.
static void _assign_ids(_table_t *table) {
  for (int i = 0, e = table->n; i < e; ++i) {
    *table->entries[i].id = i;
  }
}


/// Insert an action into a table.
///
/// @param        table The table we are inserting into.
/// @param           id The address of the user's id; written in _assign_ids().
/// @param          key The unique key for this action; read in _sort_entries().
/// @param            f The handler for this action.
/// @param         type The type of this action.
///
/// @return             HPX_SUCCESS or an error if the push fails.
static int _push_back(_table_t *table, hpx_action_t *id, const char *key,
                      hpx_action_handler_t f, _action_type_t type,
                      ffi_cif* cif) {
  static const int capacity = LIBHPX_ACTION_TABLE_SIZE;
  int i = table->n++;
  if (i >= capacity) {
    return dbg_error("exceeded maximum number of actions (%d)\n", capacity);
  }

  _entry_t *back = &table->entries[i];
  back->func = f;
  back->id = id;
  back->key = key;
  back->type = type;
  back->cif = cif;
  return HPX_SUCCESS;
}

const _table_t *action_table_finalize(void) {
  _table_t *table = _get_actions();
  _sort_entries(table);
  _assign_ids(table);
  return table;
}


void action_table_free(const _table_t *table) {
  free((void*)table);
}


const char *action_table_get_key(const struct action_table *table, hpx_action_t id) {
  if (id == HPX_INVALID_ACTION_ID) {
    dbg_log("action registration is not complete");
    return "LIBHPX UNKNOWN ACTION";
  }

  if (id < table->n) {
    return table->entries[id].key;
  }

  dbg_error("action id, %d, out of bounds [0,%u)\n", id, table->n);
  return NULL;
}


static _action_type_t action_table_get_type(const struct action_table *table, hpx_action_t id) {
  if (id == HPX_INVALID_ACTION_ID) {
    dbg_log("action registration is not complete");
    return _ACTION_UNKNOWN;
  }

  if (id < table->n) {
    return table->entries[id].type;
  }

  dbg_error("action id, %d, out of bounds [0,%u)\n", id, table->n);
  return _ACTION_UNKNOWN;
}


int action_table_get_args(const struct action_table *table, hpx_action_t id,
                          va_list inargs, void **outargs, size_t *len) {
  *outargs = NULL;
  *len = 0;

  if (id == HPX_INVALID_ACTION_ID) {
    dbg_log("action registration is not complete");
    return LIBHPX_ERROR;
  } 

  ffi_cif *cif = NULL;
  if (id < table->n) {
    cif = table->entries[id].cif;
  } else {
    dbg_error("action id, %d, out of bounds [0,%u)\n", id, table->n);
  }

  void **args = (void**)malloc(sizeof(void*) * cif->nargs);
  for (int i = 0; i < cif->nargs; ++i) {
    args[i] = va_arg(inargs, void*);
  }

  *len = ffi_raw_size(cif);
  ffi_raw *raw = (ffi_raw*)malloc(*len);
  ffi_ptrarray_to_raw(cif, args, raw);
  *outargs = (void*)raw;
  return LIBHPX_OK;
}


int action_table_run_handler(const struct action_table *table, hpx_parcel_t *parcel) {
  hpx_action_handler_t handler = 0;
  ffi_cif *cif = NULL;

  const hpx_addr_t target = hpx_parcel_get_target(parcel);
  const uint32_t owner = gas_owner_of(here->gas, target);
  DEBUG_IF (owner != here->rank) {
    dbg_log_sched("received parcel at incorrect rank, resend likely\n");
  }

  hpx_action_t id = hpx_parcel_get_action(parcel);
  void *args = hpx_parcel_get_data(parcel);

  if (id == HPX_INVALID_ACTION_ID) {
    dbg_error("action registration is not complete");
  }

  if (id < table->n) {
    handler = table->entries[id].func;
    cif = table->entries[id].cif;
  } else {
    dbg_error("action id, %d, out of bounds [0,%u)\n", id, table->n);
  }

  bool pinned = action_is_pinned(table, id);
  if (pinned) {
    hpx_gas_try_pin(target, NULL);
  }

  int ret;
  ffi_raw_call(cif, FFI_FN(handler), &ret, args);

  if (pinned) {
    hpx_gas_unpin(target);
  }

  return ret;
}


bool action_is_pinned(const struct action_table *table, hpx_action_t id) {
  return (action_table_get_type(table, id) == _ACTION_PINNED);
}


bool action_is_task(const struct action_table *table, hpx_action_t id) {
  return (action_table_get_type(table, id) == _ACTION_TASK);
}


bool action_is_interrupt(const struct action_table *table, hpx_action_t id) {
  return (action_table_get_type(table, id) == _ACTION_INTERRUPT);
}


/// Called by the user to register an action.
int hpx_register_action(hpx_action_t *id, const char *key,
                        hpx_action_handler_t f) {
  return _push_back(_get_actions(), id, key, f, _ACTION_DEFAULT, NULL);
}


int hpx_register_pinned_action(hpx_action_t *id, const char *key,
                               hpx_action_handler_t f) {
  return _push_back(_get_actions(), id, key, f, _ACTION_PINNED, NULL);
}


int hpx_register_task(hpx_action_t *id, const char *key,
                      hpx_action_handler_t f) {
  return _push_back(_get_actions(), id, key, f, _ACTION_TASK, NULL);
}


int hpx_register_interrupt(hpx_action_t *id, const char *key,
                           hpx_action_handler_t f) {
  return _push_back(_get_actions(), id, key, f, _ACTION_INTERRUPT, NULL);
}


/// Called by the user to register an action.
int hpx_register_action2(hpx_action_t *id, const char *key,
                         hpx_action_handler_t f, hpx_type_t rtype,
                         unsigned int nargs, ...) {
  va_list vargs;

  ffi_cif *cif = malloc(sizeof(*cif));
  assert(cif);

  hpx_type_t *args = malloc(sizeof(*args) * nargs);
  va_start(vargs, nargs);
  for (int i = 0; i < nargs; ++i) {
    args[i] = va_arg(vargs, hpx_type_t);
  }
  va_end(vargs);

  ffi_status s = ffi_prep_cif(cif, FFI_DEFAULT_ABI, nargs, rtype, args);
  if (s != FFI_OK) {
    dbg_error("failed to process type information for action id %d.\n", *id);
  }
  return _push_back(_get_actions(), id, key, f, _ACTION_DEFAULT, cif);
}
