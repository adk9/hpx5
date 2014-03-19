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
#ifdef CONFIG_H
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// A workstealing deque implementation based on the design presented in
/// "Dynamic Circular Work-Stealing Deque" by David Chase and Yossi Lev
/// @url http://dl.acm.org/citation.cfm?id=1073974.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdlib.h>

#include "libsync/deques.h"


chase_lev_ws_deque_t *sync_chase_lev_ws_deque_new(int size) {
  chase_lev_ws_deque_t *dq = malloc(sizeof(*dq));
  if (dq)
    sync_chase_lev_ws_deque_init(dq, size);
  return dq;
}

void sync_chase_lev_ws_deque_init(chase_lev_ws_deque_t *d, int size) {
  d->vtable.delete = (__typeof__(d->vtable.delete))sync_chase_lev_ws_deque_delete;
  d->vtable.push_left = (__typeof__(d->vtable.push_left))sync_chase_lev_ws_deque_push_left;
  d->vtable.pop_left = (__typeof__(d->vtable.pop_left))sync_chase_lev_ws_deque_pop_left;
  d->vtable.pop_left = (__typeof__(d->vtable.pop_right))sync_chase_lev_ws_deque_pop_right;
  sync_store(&d->bottom, 0, SYNC_RELAXED);
  sync_store(&d->top, 0, SYNC_RELAXED);
  sync_store(&d->size, size, SYNC_RELAXED);
  void **buffer = calloc(size, sizeof(buffer[0]));
  assert(buffer);
  sync_store(&d->buffer, buffer, SYNC_RELAXED);
}

void sync_chase_lev_ws_deque_fini(chase_lev_ws_deque_t *d) {
  void **buffer = (void**)sync_load(&d->buffer, SYNC_RELAXED);
  if (buffer)
    free(buffer);
  sync_store(&d->size, 0, SYNC_RELAXED);
}

void sync_chase_lev_ws_deque_delete(chase_lev_ws_deque_t *d) {
  if (!d)
    return;
  sync_chase_lev_ws_deque_fini(d);
  free(d);
}

void sync_chase_lev_ws_deque_push_left(chase_lev_ws_deque_t *d, void *val) {
}

void *sync_chase_lev_ws_deque_pop_left(chase_lev_ws_deque_t *d) {
  return NULL;
}

void *sync_chase_lev_ws_deque_pop_right(chase_lev_ws_deque_t *d) {
  return NULL;
}
