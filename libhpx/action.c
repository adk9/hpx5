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
#include "libhpx/utils.h"
#include "libsync/hashtables.h"

#ifdef ENABLE_TAU
#define TAU_DEFAULT 1
#include <TAU.h>
#endif
// Some constants that we use to govern the behavior of the action
// table:

// initial table size
static const int ACTIONS_INITIAL_HT_SIZE = 256;

// when to expand the table
static const int ACTIONS_PROBE_LIMIT = 2;

// The hashtable entry type.
//
// Our hashtable is just an array of key-value pairs, this is the type
// of that array element.
struct _entry {
  long  key;
  const void *value;
};

// The action hashtable is a linear probed hashtable, i.e., an array.
typedef struct _action_table _action_table_t;
static struct _action_table {
  size_t         size;
  struct _entry *table;
} _action_table;

void _expand(_action_table_t *ht);
int _insert(_action_table_t *ht, const long key, const void *value);

// Expand a hashtable.
//
// The performance of this routine isn't important because it only happens once
// per node, while actions are being inserted. It may be called recursively (via
// the insert() routine) when it encounters a collision so that lookups never
// collide.
//
// @param[in] ht - the hashtable to expand
void _expand(_action_table_t *ht) {
  assert(ht != NULL);

  // remember the previous state of the table
  const int e = ht->size;
  struct _entry *copy = ht->table;

  // double the size of the table
  ht->size = 2 * ht->size;
  ht->table = calloc(ht->size, sizeof(ht->table[0]));

  // iterate through the previous table, and insert all of the values
  // that aren't empty---i.e., anything where entry->value isn't NULL.
  // This could trigger recursive expansion, but that's ok, because this
  // loop will insert anything that wasn't inserted in the inner
  // expansion.
  for (int i = 0; i < e; ++i) {
    if (copy[i].value != NULL)
      _insert(ht, copy[i].key, copy[i].value);
  }

  // don't need copy anymore
  free(copy);
}


// Insert a key-value pair into a hashtable.
//
// We don't really care about the performance of this operation because of the
// two-phased approach to the way that we use the hashtable, all inserts happen
// once, during table initialization, and then this is read-only.
//
// @param[in] ht    - the hashtable
// @param[in] key   - the action key to insert
// @param[in] value - the local function pointer for the action
//
// @returns key
int _insert(_action_table_t *ht, const long key, const void *value) {
  assert(ht != NULL);
  assert(key != 0);
  assert(value != NULL);

  // lazy initialization of the action table
  if (!ht->table) {
    ht->size = ACTIONS_INITIAL_HT_SIZE;
    ht->table = calloc(ACTIONS_INITIAL_HT_SIZE, sizeof(ht->table[0]));
  }

  size_t i = key % ht->size;
  size_t j = 0;

  // search for the correct bucket, which is just a linear search, bounded by
  // ACTIONS_PROBE_LIMIT
  while (ht->table[i].key != 0) {
    assert(((ht->table[i].key != key) || (ht->table[i].value == value)) &&
           "attempting to overwrite key during registration");

    i = (i + 1) % ht->size;                     // linear probing
    j = j + 1;

    if (ACTIONS_PROBE_LIMIT < j) {              // stop probing
      _expand(ht);
      i = key % ht->size;
      j = 0;
    }
  }

  // insert the entry
  ht->table[i].key = key;
  ht->table[i].value = value;
  return 1;
}


// Hashtable lookup.
//
// Implement a simple, linear probed table. Entries with a key of 0 or value of
// NULL are considered invalid, and terminate the search.
const void* _lookup(const _action_table_t *ht, const long key) {
  assert(ht != NULL);
  assert(key != 0);

  // We just keep probing here until we hit an invalid entry, because
  // we know that the probe limit was enforced during insert.
  size_t i = key % ht->size;
  while ((ht->table[i].key != 0) && (ht->table[i].key != key))
    i = (i + 1) % ht->size;
  return ht->table[i].value;
}

#ifdef ENABLE_DEBUG
typedef struct {
  hpx_action_handler_t f;
  const char *name;
} _action_entry_t;

// Insert an action entry with the function name @p key necessary for
// debugging or tracing actions.
int _dbg_action_insert(const long key, const hpx_action_handler_t f, const char *name) {
  _action_entry_t *entry = malloc(sizeof(*entry));
  entry->f = f;
  entry->name = name;
  return _insert(&_action_table, key, entry);
}
#endif


bool hpx_action_eq(const hpx_action_t lhs, const hpx_action_t rhs) {
  return (lhs == rhs);
}


hpx_action_t action_register(const char *key, hpx_action_handler_t f) {
#ifdef ENABLE_ACTION_TABLE
  int e;
  size_t len = strlen(key);
  const long hkey = hpx_hash_string(key, len);
#if ENABLE_DEBUG
  e = _dbg_action_insert(hkey, f, key);
#else
  e = _insert(&_action_table, hkey, (const void*)(hpx_action_t)f);
#endif
  assert(e);
  return (hpx_action_t)hkey;
#else
#if ENABLE_DEBUG
  int e = _dbg_action_insert((long)f, f, key);
  assert(e);
#endif
  return (hpx_action_t)f;
#endif
}


hpx_action_handler_t action_lookup(hpx_action_t id) {
#ifdef ENABLE_ACTION_TABLE
  const void *f = _lookup(&_action_table, (long)id);
  assert(f);
#ifdef ENABLE_DEBUG
  return ((_action_entry_t*)f)->f;
#endif
  return (hpx_action_handler_t)(hpx_action_t)f;
#endif
  return (hpx_action_handler_t)id;
}


int action_invoke(hpx_action_t action, void *args) {
  hpx_action_handler_t handler = action_lookup(action);
  return handler(args);
}


const char *action_get_key(hpx_action_t id) {
  const char *key = NULL;
#ifdef ENABLE_DEBUG
  const _action_entry_t *entry = _lookup(&_action_table, id);
  if (entry)
    key = entry->name;
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
