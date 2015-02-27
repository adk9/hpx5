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
#ifndef LIBHPX_DEQUES_H
#define LIBHPX_DEQUES_H

/// ----------------------------------------------------------------------------
/// @file include/libsync/deques.h
/// ----------------------------------------------------------------------------
#include <stddef.h>
#include <stdint.h>

#include <hpx/attributes.h>
#include "libsync/sync.h"

/// A workstealing deque implementation based on the design presented in
/// "Dynamic Circular Work-Stealing Deque" by David Chase and Yossi Lev
/// @url http://dl.acm.org/citation.cfm?id=1073974.
struct chase_lev_ws_deque_buffer;

typedef struct chase_lev_ws_deque {
  volatile uint64_t bottom;
  uint64_t top_bound;
  struct chase_lev_ws_deque_buffer* volatile buffer;
  const char _PADA[64 - 24];
  volatile uint64_t top;
} chase_lev_ws_deque_t;

#define SYNC_CHASE_LEV_WS_DEQUE_INIT {          \
    .PADA = {0},\
  .bottom = 1,                                  \
 .top_bound = 1                                 \
  .buffer = NULL,                               \
     .top = 1                                   \
    }

chase_lev_ws_deque_t *sync_chase_lev_ws_deque_new(uint32_t capacity)
  SYNC_INTERNAL;

void sync_chase_lev_ws_deque_init(chase_lev_ws_deque_t *d, uint32_t capacity)
  SYNC_INTERNAL HPX_NON_NULL(1);

void sync_chase_lev_ws_deque_fini(chase_lev_ws_deque_t *d)
  SYNC_INTERNAL HPX_NON_NULL(1);

void sync_chase_lev_ws_deque_delete(chase_lev_ws_deque_t *d)
  SYNC_INTERNAL;

/// Query the size of the deque.
///
/// This is a good estimate of the size of the deque, as it will sync_load the
/// bottom and the top. This will not update the top bound.
uint64_t sync_chase_lev_ws_deque_size(chase_lev_ws_deque_t *d)
  SYNC_INTERNAL HPX_NON_NULL(1);

/// Pushes an item onto the deque.
///
/// This will return the current estimate of the size of the deque for callers
/// to use. This is only an approximate value.
uint64_t sync_chase_lev_ws_deque_push(chase_lev_ws_deque_t *d, void *val)
  SYNC_INTERNAL HPX_NON_NULL(1);

void *sync_chase_lev_ws_deque_pop(chase_lev_ws_deque_t *d)
  SYNC_INTERNAL HPX_NON_NULL(1);

void *sync_chase_lev_ws_deque_steal(chase_lev_ws_deque_t *d)
  SYNC_INTERNAL HPX_NON_NULL(1);

#endif // LIBHPX_DEQUES_H
