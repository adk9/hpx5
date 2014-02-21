/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Actions
  action.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>                             /* assert() */
#include <stddef.h>                             /* NULL */
#include <stdlib.h>                             /* calloc/free */
#include <string.h>                             /* memcpy */

#include "hpx/action.h"
#include "action.h"                             /* action_lookup */
#include "hpx/globals.h"                        /* __hpx_global_ctx */
#include "hpx/lco.h"
#include "hpx/parcel.h"                         /* hpx_parcel stuff */
#include "debug.h"                              /* dbg_* stuff */
#include "hashstr.h"                            /* hashstr() */
#include "network.h"                            /* struct network_mgr */
#include "libhpx/parcel.h"                      /* struct hpx_parcel */

/** Typedefs that we use for convenience in this source. */
typedef struct hpx_parcel   parcel_t;
typedef struct hpx_locality locality_t;
typedef struct hpx_future   future_t;
/** @} */

/**
 * Some constants that we use to govern the behavior of the action table.
 * @{
 */
static const int ACTIONS_INITIAL_HT_SIZE = 256; /**< initial table size */
static const int ACTIONS_PROBE_LIMIT     = 2;   /**< when to expand table */
/**
 * @}
 */

/**
 * The hashtable entry type.
 *
 * Our hashtable is just an array of key-value pairs, this is the type of that
 * array element.
 */
struct entry {
  hpx_action_t  key;
  hpx_func_t value;
};

/**
 * The action hashtable is a linear probed hashtable, i.e., an array.
 */
static struct hashtable {
  size_t        size;
  struct entry *table;
} actions;

/**
 * Action registration completion future. Needs to be visible elsewhere.
 */
struct hpx_future *action_registration_complete; /* not static since actual allocation and initialization is done by hpx_init() */

static void         expand(struct hashtable *);
static hpx_action_t insert(struct hashtable *, const hpx_action_t, const hpx_func_t);
static hpx_func_t   lookup(const struct hashtable *, const hpx_action_t);

/**
 * Expand a hashtable.
 *
 * The performance of this routine isn't important because it only happens once
 * per node, while actions are being inserted. It may be called recursively (via
 * the insert() routine) when it encounters a collision so that lookups never
 * collide.
 *
 * @param[in] ht - the hashtable to expand
 */
void
expand(struct hashtable *ht)
{
  dbg_assert_precondition(ht != NULL);
  
  /* remember the previous state of the table */
  const int e = ht->size;
  struct entry *copy = ht->table;

  /* double the size of the table */
  ht->size = 2 * ht->size;
  ht->table = calloc(ht->size, sizeof(ht->table[0]));

  /* iterate through the previous table, and insert all of the values that
     aren't empty---i.e., anything where entry->value isn't NULL. This could
     trigger recursive expansion, but that's ok, because this loop will insert
     anything that wasn't inserted in the inner expansion. */
  for (int i = 0; i < e; ++i)
    if (copy[i].value != NULL)
      insert(ht, copy[i].key, copy[i].value);

  /* don't need copy anymore */
  free(copy);
}

/**
 * Insert a key-value pair into a hashtable.
 *
 * We don't really care about the performance of this operation because of the
 * two-phased approach to the way that we use the hashtable, all inserts happen
 * once, during table initialization, and then this is read-only.
 *
 * @param[in] ht    - the hashtable
 * @param[in] key   - the action key to insert
 * @param[in] value - the local function pointer for the action
 *
 * @returns key
 */
hpx_action_t
insert(struct hashtable *ht, const hpx_action_t key, const hpx_func_t value)
{
  dbg_assert_precondition(ht != NULL);
  dbg_assert_precondition(key != 0);
  dbg_assert_precondition(value != NULL);
  
  /* lazy initialization of the action table */
  if (!ht->table) {
    ht->size = ACTIONS_INITIAL_HT_SIZE;
    ht->table = calloc(ACTIONS_INITIAL_HT_SIZE, sizeof(ht->table[0]));
  }
  
  size_t i = key % ht->size;
  size_t j = 0;

  /* search for the correct bucket, which is just a linear search, bounded by
     ACTIONS_PROBE_LIMIT */ 
  while (ht->table[i].key != 0) {
    assert(((ht->table[i].key != key) || (ht->table[i].value == value)) && 
           "attempting to overwrite key during registration");
    
    i = (i + 1) % ht->size;                     /* linear probing */
    j = j + 1;
    
    if (ACTIONS_PROBE_LIMIT < j) {              /* stop probing */
      expand(ht);
      i = key % ht->size;
      j = 0;
    }
  }

  /* insert the entry */
  ht->table[i].key = key;
  ht->table[i].value = value;
  return key;
}

/**
 * Hashtable lookup.
 *
 * Implement a simple, linear probed table. Entries with a key of 0 or value of
 * NULL are considered invalid, and terminate the search.
 */
hpx_func_t
lookup(const struct hashtable *ht, const hpx_action_t key)
{
  dbg_assert_precondition(ht != NULL);
  dbg_assert_precondition(key != 0);

  /* We just keep probing here until we hit an invalid entry, because we know
   * that the probe limit was enforced during insert. */
  size_t i = key % ht->size;
  while ((ht->table[i].key != 0) && (ht->table[i].key != key))
    i = (i + 1) % ht->size;
  return ht->table[i].value;
}

/************************************************************************/
/* ADK: There are a few ways to handle action registration--The         */
/* simplest is under the naive assumption that we are executing in a    */
/* homogeneous, SPMD environment and parcels simply carry function      */
/* pointers around. The second is to have all interested localities     */
/* register the required functions and then simply pass tags            */
/* around. Finally, a simpler, yet practical alternative, is to have a  */
/* local registration scheme for exported functions. Eventually, we     */
/* would want to have a distributed namespace for parcels that provides */
/* all three options.                                                   */
/************************************************************************/


hpx_func_t
action_lookup(hpx_action_t key)
{
  return lookup(&actions, key);
}

/**
 * Register an HPX action.
 * 
 * @param[in] name - Action Name
 * @param[in] func - The HPX function that is represented by this action.
 * 
 * @return HPX error code
 */
hpx_action_t
hpx_action_register(const char *name, hpx_func_t func)
{
  dbg_assert_precondition(name != NULL);
  dbg_assert_precondition(func != NULL);
  return insert(&actions, hashstr(name), func);
}

/**
 * Called after all action registration is complete.
 */
void 
hpx_action_registration_complete(void) {
  hpx_lco_future_set_state(action_registration_complete);
}

/**
 * Called to determine if action registration is complete.
 */
bool 
hpx_is_action_registration_complete(void) {
  return hpx_lco_future_isset(action_registration_complete);
}

/**
 * Called to wait for action registration to complete.
 */
void 
hpx_waitfor_action_registration_complete(void) {
  hpx_thread_wait(action_registration_complete);
}

/**
 * Call to invoke an action locally.
 *
 * @param[in]  action - the action id we want to perform
 * @param[in]  args   - the argument buffer for the action
o * @param[in]  thread - a future that will be triggered with the address of the
 *                      remote thread when the send is complete, may be NULL
 * @param[out] out    - a future to wait on, may be NULL
 *
 * @returns an error code
 */
hpx_error_t
hpx_action_invoke(hpx_action_t action, void *args,  hpx_future_t *thread, future_t **out)
{
  hpx_func_t f = action_lookup(action);
  dbg_assert(f && "Failed to find action");
  hpx_thread_t *th;
  hpx_error_t success = hpx_thread_create(__hpx_global_ctx, 0, f, args, out, &th);
  if (thread != NULL)
    hpx_lco_future_set(thread, HPX_LCO_FUTURE_SETMASK, th);
  return success;
}

static void action_wrap(void *arg) {
  struct hpx_parcel *parcel = (struct hpx_parcel*)arg;
  hpx_func_t f = action_lookup(parcel->action);
  dbg_assert(f && "Failed to find action");
  f(parcel->payload);
  hpx_parcel_release(parcel);
  // TODO call thread_exit() to set the result future?
}

/**
 * Call to invoke an action locally.
 *
 * @param[in]  parcel - the parcel that contains the action and arguments
 * @param[in]  thread - a future that will be triggered with the address of the
 *                      remote thread when the send is complete, may be NULL
 * @param[out] out    - a future to wait on, may be NULL
 *
 * @returns an error code
 */
hpx_error_t
hpx_action_invoke_parcel(struct hpx_parcel *parcel, hpx_future_t *thread, future_t **out)
{
  hpx_func_t f = action_lookup(parcel->action);
  dbg_assert(f && "Failed to find action");
  if (f!= NULL) {
    hpx_thread_t *th;
    hpx_error_t success = hpx_thread_create(__hpx_global_ctx, 0, (hpx_func_t)action_wrap, parcel, out, &th);
    if (thread != NULL)
      hpx_lco_future_set(thread, HPX_LCO_FUTURE_SETMASK, th);
    return success;
  }
  return HPX_ERROR;
}

/**
 * Call to perform a possibly-remote procedure call.
 *
 * @param[in] dest   - the destination locality
 * @param[in] action - the action to invoke
 * @param[in] args - the argument data buffer
 * @param[in] len - the length of the argument data buffer
 * @param[in] result - a future for the RPC result (if desired)
 *
 * @returns HPX_SUCCESS or an error code
 */
hpx_error_t
hpx_call(locality_t *dest, hpx_action_t action, void *args, size_t len,
         future_t **result)
{
  dbg_assert_precondition(dest);
  dbg_assert_precondition(action);
  dbg_assert_precondition(((len && args) || ((args && len) == 0)));

  parcel_t *p = hpx_parcel_acquire(len);
  if (!p) {
    dbg_printf("Could not allocate a %lu-byte parcel in hpx_call.\n", len);
    return HPX_ERROR;
  }
  
  hpx_parcel_set_action(p, action);
  hpx_parcel_set_data(p, args, len);
  int success = hpx_parcel_send(dest, p, NULL, result);
  return success;
}
