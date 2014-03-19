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
#ifndef LIBHPX_DEQUES_H
#define LIBHPX_DEQUES_H

/// ----------------------------------------------------------------------------
/// @file include/libsync/deques.h
/// ----------------------------------------------------------------------------
#include "hpx/attributes.h"


typedef struct ws_deque {
  void (*delete)(struct ws_deque*) HPX_NON_NULL(1);
  void (*push_left)(struct ws_deque*, void *) HPX_NON_NULL(1);
  void *(*pop_left)(struct ws_deque*) HPX_NON_NULL(1);
  void *(*pop_right)(struct ws_deque*) HPX_NON_NULL(1);
} ws_deque_t;

static inline void sync_ws_deque_delete(ws_deque_t *d) {
  d->delete(d);
}

static inline void sync_ws_deque_push_left(ws_deque_t *d, void *val) {
  d->push(d, val);
}

static inline void *sync_ws_deque_pop_left(ws_deque *d) {
  return d->pop_left(d);
}

static inline void *sync_ws_deque_pop_right(ws_deque *d) {
  return d->pop_right(d);
}

typedef struct chase_lev_ws_deque {
  ws_deque vtable;
} chase_lev_ws_deque_t;

void sync_chase_lev_ws_deque_init(chase_lev_ws_deque_t *d);
void sync_chase_lev_ws_deque_fini(chase_lev_ws_deque_t *d);
void sync_chase_lev_ws_deque_delete(chase_lev_ws_deque_t *d);
void sync_chase_lev_ws_deque_push_left(chase_lev_ws_deque_t *d, void *val);
void *sync_chase_lev_ws_deque_pop_left(chase_lev_ws_deque *d);
void *sync_chase_lev_ws_deque_pop_right(chase_lev_ws_deque *d);

#endif // LIBHPX_DEQUES_H
