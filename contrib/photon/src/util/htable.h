#ifndef HTABLE_H
#define HTABLE_H

#include <stdint.h>
#include "libsync/locks.h"

typedef struct hash_element {
  uint64_t key;
  void *value;
  struct hash_element *prev;
  struct hash_element *next;
} hash_element_t;

typedef struct htable {
  hash_element_t **table;
  int size;
  int elements;
  tatas_lock_t tlock;
} htable_t;

htable_t *htable_create(int size);
int htable_insert(htable_t *table, uint64_t key, void *value);
int htable_update(htable_t *table, uint64_t key, void *value, void **old_value);
int htable_update_if_exists(htable_t *table, uint64_t key, void *value, void **old_value);
int htable_lookup(htable_t *table, uint64_t key, void **value);
int htable_remove(htable_t *table, uint64_t key, void **value);
int htable_count(htable_t *table);
void htable_free(htable_t *table);
void htable_print(htable_t *htable);

#endif
