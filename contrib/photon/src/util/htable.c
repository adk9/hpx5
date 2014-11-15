#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "htable.h"

static hash_element_t *__htable_lookup(htable_t *htable, uint64_t key);
static int hash_key(uint64_t key, int table_size);

htable_t *htable_create(int size) {
  htable_t *htable;

  htable = (htable_t *)malloc(sizeof(htable_t));
  if( !htable )
    goto error_exit;

  htable->table = malloc(sizeof(hash_element_t *) * size);
  if (!htable->table)
    goto error_exit2;

  memset(htable->table, 0, sizeof(hash_element_t *) * size);

  htable->size = size;
  htable->elements = 0;
  pthread_mutex_init(&htable->mtx, NULL);

  return htable;

error_exit2:
  free(htable);
error_exit:
  return NULL;

}


static int hash_key(uint64_t key, int table_size) {
  return (int)(key%table_size);

}


int htable_insert(htable_t *htable, uint64_t key, void *value) {
  int hash;
  hash_element_t *temp;

  pthread_mutex_lock(&htable->mtx);
  {
    // TODO: Do we need to verify that the element doesn't exist?
    if (__htable_lookup(htable, key) != NULL) {
      pthread_mutex_unlock(&htable->mtx);
      return -2;
    }

    // allocate a new node
    temp = malloc( sizeof(hash_element_t) );
    temp->key = key;
    temp->value = value;
    temp->next = NULL;
    temp->prev = NULL;

    // insert the node at the head of the list
    hash = hash_key(key, htable->size);
    temp->next = htable->table[hash];
    if (temp->next)
      temp->next->prev = temp;
    htable->table[hash] = temp;

    htable->elements++;
  }
  pthread_mutex_unlock(&htable->mtx);

  return 0;
}

static hash_element_t *__htable_lookup(htable_t *htable, uint64_t key) {
  hash_element_t *temp;
  int hash;

  hash = hash_key(key, htable->size);

  for(temp = htable->table[hash]; temp != NULL; temp = temp->next) {
    if( temp->key == key )
      return temp;
  }

  return NULL;
}

int htable_lookup(htable_t *htable, uint64_t key, void **value) {
  hash_element_t *temp;

  pthread_mutex_lock(&htable->mtx);
  {
    temp = __htable_lookup(htable, key);
    if (!temp) {
      pthread_mutex_unlock(&htable->mtx);
      return -1;
    }

    if (value)
      *value = temp->value;
  }
  pthread_mutex_unlock(&htable->mtx);

  return 0;
}


int htable_remove(htable_t *htable, uint64_t key, void **value) {
  hash_element_t *temp;
  int hash;

  pthread_mutex_lock(&htable->mtx);
  {
    temp = __htable_lookup(htable, key);
    if (!temp) {
      pthread_mutex_unlock(&htable->mtx);
      return -1;
    }

    // remove the node
    if (temp->prev)
      temp->prev->next = temp->next;
    if (temp->next)
      temp->next->prev = temp->prev;

    // if the node was the head of the list
    if (temp->prev == NULL) {
      hash = hash_key(key, htable->size);
      htable->table[hash] = temp->next;
    }

    // return the value to the user if they want it
    if (value != NULL)
      *value = temp->value;

    free(temp);

    htable->elements--;
  }
  pthread_mutex_unlock(&htable->mtx);

  return 0;
}

void htable_free(htable_t *htable) {
  int i;
  hash_element_t *temp;

  for(i = 0; i < htable->size; i++) {
    temp = htable->table[i];
    while(temp != NULL) {
      hash_element_t *next = temp->next;
      free(temp);
      temp = next;
    }
  }

  free(htable);
}

void htable_print(htable_t *htable) {
  int i;
  hash_element_t *temp;

  for(i = 0; i < htable->size; i++) {
    printf("hash[%d]:", i);
    for(temp = htable->table[i]; temp != NULL; temp = temp->next) {
      printf(" %llu", (long long unsigned int) temp->key);
    }
    printf("\n");
  }
}

int htable_count(htable_t *htable) {
  return htable->elements;
}

int htable_update(htable_t *htable, uint64_t key, void *value, void **old_value) {
  hash_element_t *temp;

  pthread_mutex_lock(&htable->mtx);
  {
    temp = __htable_lookup(htable, key);
    if (!temp) {
      pthread_mutex_unlock(&htable->mtx);
      return htable_insert(htable, key, value);
    }

    if (old_value)
      old_value = temp->value;
    temp->value = value;
  }
  pthread_mutex_unlock(&htable->mtx);

  return 1;
}

int htable_update_if_exists(htable_t *htable, uint64_t key, void *value, void **old_value) {
  hash_element_t *temp;

  pthread_mutex_lock(&htable->mtx);
  {
    temp = __htable_lookup(htable, key);
    if (!temp) {
      pthread_mutex_unlock(&htable->mtx);
      return -1;
    }

    if (old_value)
      *old_value = temp->value;
    temp->value = value;
  }
  pthread_mutex_unlock(&htable->mtx);

  return 1;
}
