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

/// A workstealing deque implementation based on the design presented in
/// "Dynamic Circular Work-Stealing Deque" by David Chase and Yossi Lev
/// @url http://dl.acm.org/citation.cfm?id=1073974.
struct chase_lev_ws_deque_buffer;

typedef struct chase_lev_ws_deque {
  volatile uint64_t bottom;
  volatile uint64_t top;
  struct chase_lev_ws_deque_buffer* volatile buffer;
  uint64_t top_bound;
} chase_lev_ws_deque_t;

#define SYNC_CHASE_LEV_WS_DEQUE_INIT {          \
    .bottom = 1,                                \
    .top = 1,                                   \
    .buffer = NULL,                             \
    .top_bound = 1                              \
    }

HPX_INTERNAL chase_lev_ws_deque_t *sync_chase_lev_ws_deque_new(size_t capacity);

HPX_INTERNAL void sync_chase_lev_ws_deque_init(chase_lev_ws_deque_t *d,
                                               size_t capacity)
  HPX_NON_NULL(1);

HPX_INTERNAL void sync_chase_lev_ws_deque_fini(chase_lev_ws_deque_t *d)
  HPX_NON_NULL(1);

HPX_INTERNAL void sync_chase_lev_ws_deque_delete(chase_lev_ws_deque_t *d)
  HPX_NON_NULL(1);

HPX_INTERNAL void sync_chase_lev_ws_deque_push(chase_lev_ws_deque_t *d,
                                               void *val)
  HPX_NON_NULL(1);

HPX_INTERNAL void *sync_chase_lev_ws_deque_pop(chase_lev_ws_deque_t *d)
  HPX_NON_NULL(1);

HPX_INTERNAL void *sync_chase_lev_ws_deque_steal(chase_lev_ws_deque_t *d)
  HPX_NON_NULL(1);

#endif // LIBHPX_DEQUES_H
