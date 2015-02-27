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
#include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include "libsync/hashtables.h"
#include "uthash.h"

struct cuckoo_bucket {
  long key;
  const void *value;
  UT_hash_handle hh;                            // HACK
};


static void HPX_NON_NULL(1) _cuckoo_lock(cuckoo_hashtable_t *ht) {
  sync_tatas_acquire(&ht->lock);
}


static void HPX_NON_NULL(1) _cuckoo_unlock(cuckoo_hashtable_t *ht) {
  sync_tatas_release(&ht->lock);
}


cuckoo_hashtable_t *
sync_cuckoo_hashtable_new(void) {
  cuckoo_hashtable_t *ht = malloc(sizeof(*ht));
  if (ht)
    sync_cuckoo_hashtable_init(ht);
  return ht;
}


void
sync_cuckoo_hashtable_init(cuckoo_hashtable_t *ht) {
  sync_tatas_init(&ht->lock);
  ht->size = 0;
  ht->table = NULL;
}


void
sync_cuckoo_hashtable_delete(cuckoo_hashtable_t *ht) {
  if (!ht)
    return;
  _cuckoo_lock(ht);
  free(ht);
}


int
sync_cuckoo_hashtable_insert(cuckoo_hashtable_t *ht, long key, const void *value) {
  _cuckoo_lock(ht);
  cuckoo_bucket_t *entry = malloc(sizeof(*entry));
  assert(entry);
  entry->key = key;
  entry->value = value;
  HASH_ADD_INT(ht->table, key, entry);
  _cuckoo_unlock(ht);
  return 1;
}


const void *
sync_cuckoo_hashtable_lookup(cuckoo_hashtable_t *ht, long key) {
  cuckoo_bucket_t *entry;
  _cuckoo_lock(ht);
  HASH_FIND_INT(ht->table, &key, entry);
  _cuckoo_unlock(ht);
  if (!entry)
    return NULL;
  return entry->value;
}


void
sync_cuckoo_hashtable_remove(cuckoo_hashtable_t *ht, long key) {
  _cuckoo_lock(ht);
  cuckoo_bucket_t *entry;
  HASH_FIND_INT(ht->table, &key, entry);
  if (entry) {
    HASH_DEL(ht->table, entry);
    free(entry);
  }
  _cuckoo_unlock(ht);
}

