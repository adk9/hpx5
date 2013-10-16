/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Parcel Registration
  register.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "hpx/init.h"
#include "hpx/action.h"
#include "hpx/parcel.h"
#include "hashstr.h"
#include "network/network.h"

static const int ACTIONS_INITIAL_HT_SIZE = 256;
static const int ACTIONS_PROBE_LIMIT = 3;

struct entry {
  hpx_action_t  key;
  hpx_func_t value;
};

static struct hashtable {
  size_t        size;
  struct entry *table;
} actions;

static void expand(struct hashtable * const);
static hpx_action_t insert(struct hashtable * const,
                           const hpx_action_t, const hpx_func_t);
static hpx_func_t lookup(const struct hashtable * const ,
                         const hpx_action_t);

/**
 * Slow expansion routine. Not important because it does what it needs to
 * do.
 */
void expand(struct hashtable * const ht) {
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
  for (int i = 0; i < e; i++)
    if (copy[i].value != NULL)
      insert(ht, copy[i].key, copy[i].value);

  /* don't need copy anymore */
  free(copy);
}

/**
 * Insert a key-value pair. We don't really care about the performance of this
 * operation because of the two-phased approach to the way that we use the
 * hashtable. 
 */
hpx_action_t insert(struct hashtable * const ht, const hpx_action_t key,
                    const hpx_func_t value) {
  /* preconditions */
  assert(ht && "inserting into null hashtable");
  assert(value != NULL && "inserting invalid value (NULL indicates empty)");
  
  /* lazy initialization of the action table */
  if (!ht->table) {
    ht->size = ACTIONS_INITIAL_HT_SIZE;
    ht->table = calloc(ACTIONS_INITIAL_HT_SIZE, sizeof(ht->table[0]));
  }
  
  size_t i = key % ht->size;
  size_t j = 0;

  /* search for the correct bucket, which is just a linear search, bounded by
     ACTIONS_PROBE_LIMIT */ 
  while (ht->table[i].value != NULL) {
    /* error checking */
    assert(((ht->table[i].key != key) || (ht->table[i].value == value)) && 
           "attempting to overwrite key during registration, MD5 collision?");
    
    i = (i + 1) % ht->size;
    j = j + 1;
    
    if (ACTIONS_PROBE_LIMIT < j) {
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
 * Our table is a simple, linear probed table. Entries with a value of NULL
 * are considered invalid, and terminate the search. Our Hashtable is used
 * in a simple, two-phased approach where all inserts happen before any
 * lookups. We could implement a perfect hashing routine for this, but for
 * now we're using this simple option.
 */
static hpx_func_t lookup(const struct hashtable *ht, const hpx_action_t key) {
  /* preconditions */
  assert(ht && "looking up in NULL hashtable");
  
  size_t i = key % ht->size;
  do {
    if (ht->table[i].key == key)
      return ht->table[i].value;
    i = (i + 1) % ht->size; 
  } while (ht->table[i].value != NULL);
  return NULL;
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

/** 
 * Register an HPX action.
 * 
 * @param name Action Name
 * @param func The HPX function that is represented by this action.
 * 
 * @return HPX error code
 */
hpx_action_t hpx_action_register(const char *name, hpx_func_t func) {
  /* preconditions */
  assert(name && "cannot use null name during registration");
  assert(func && "cannot register NULL action");
  return insert(&actions, hashstr(name), func);
}

hpx_func_t hpx_action_lookup_local(hpx_action_t action) {
  return lookup(&actions, action);
}

hpx_future_t* hpx_action_invoke(hpx_action_t action, void *args,
                                hpx_thread_t** thp) {
  // spawn a thread to invoke the action locally
  hpx_func_t f = hpx_action_lookup_local(action);
  return (f) ? hpx_thread_create(__hpx_global_ctx, 0, f, args, thp) : NULL;
}

void hpx_action_registration_complete(void) {
  hpx_network_barrier();
}
