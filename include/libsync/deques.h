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
#include <stddef.h>
#include <stdint.h>

#include "hpx/attributes.h"
#include "libsync/sync.h"

typedef struct ws_deque {
  void (*delete)(struct ws_deque*) HPX_NON_NULL(1);
  void (*push_left)(struct ws_deque*, void *) HPX_NON_NULL(1);
  void *(*pop_left)(struct ws_deque*) HPX_NON_NULL(1);
  void *(*pop_right)(struct ws_deque*) HPX_NON_NULL(1);
} ws_deque_t;

static inline HPX_NON_NULL(1) void sync_ws_deque_delete(ws_deque_t *d) {
  d->delete(d);
}

static inline HPX_NON_NULL(1) void sync_ws_deque_push_left(ws_deque_t *d,
                                                           void *val) {
  d->push_left(d, val);
}

static inline HPX_NON_NULL(1) void *sync_ws_deque_pop_left(ws_deque_t *d) {
  return d->pop_left(d);
}

static inline HPX_NON_NULL(1) void *sync_ws_deque_pop_right(ws_deque_t *d) {
  return d->pop_right(d);
}

typedef struct chase_lev_ws_deque {
  ws_deque_t vtable;
  SYNC_ATOMIC(uint64_t) bottom;
  SYNC_ATOMIC(uint64_t) top;
  SYNC_ATOMIC(size_t) size;
  void * SYNC_ATOMIC(*) array;
} chase_lev_ws_deque_t;

HPX_INTERNAL chase_lev_ws_deque_t * sync_chase_lev_ws_deque_new(int size);

HPX_INTERNAL void sync_chase_lev_ws_deque_init(chase_lev_ws_deque_t *d,
                                               int size)
  HPX_NON_NULL(1);

HPX_INTERNAL void sync_chase_lev_ws_deque_fini(chase_lev_ws_deque_t *d)
  HPX_NON_NULL(1);

HPX_INTERNAL void sync_chase_lev_ws_deque_delete(chase_lev_ws_deque_t *d)
  HPX_NON_NULL(1);

HPX_INTERNAL void sync_chase_lev_ws_deque_push_left(chase_lev_ws_deque_t *d,
                                                    void *val)
  HPX_NON_NULL(1);

HPX_INTERNAL void *sync_chase_lev_ws_deque_pop_left(chase_lev_ws_deque_t *d)
  HPX_NON_NULL(1);

HPX_INTERNAL void *sync_chase_lev_ws_deque_pop_right(chase_lev_ws_deque_t *d)
  HPX_NON_NULL(1);

#endif // LIBHPX_DEQUES_H
