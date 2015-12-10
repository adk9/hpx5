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

#ifndef LIBHPX_LIBHPX_ACTIONS_TABLE_H
#define LIBHPX_LIBHPX_ACTIONS_TABLE_H

#include <hpx/hpx.h>

void entry_init_execute_parcel(action_entry_t *entry);
void entry_init_pack_buffer(action_entry_t *entry);
void entry_init_new_parcel(action_entry_t *entry);

#ifdef ENABLE_DEBUG
void CHECK_BOUND(const action_entry_t *table, hpx_action_t id);
#else
#define CHECK_BOUND(table, id)
#endif

static inline bool entry_is_pinned(const action_entry_t *e) {
  return (e->attr & HPX_PINNED);
}

static inline bool entry_is_marshalled(const action_entry_t *e) {
  return (e->attr & HPX_MARSHALLED);
}

static inline bool entry_is_vectored(const action_entry_t *e) {
  return (e->attr & HPX_VECTORED);
}

static inline bool entry_is_internal(const action_entry_t *e) {
  return (e->attr & HPX_INTERNAL);
}

static inline bool entry_is_default(const action_entry_t *e) {
  return (e->type == HPX_DEFAULT);
}

static inline bool entry_is_task(const action_entry_t *e) {
  return (e->type == HPX_TASK);
}

static inline bool entry_is_interrupt(const action_entry_t *e) {
  return (e->type == HPX_INTERRUPT);
}

static inline bool entry_is_function(const action_entry_t *e) {
  return (e->type == HPX_FUNCTION);
}

static inline bool entry_is_opencl(const action_entry_t *e) {
  return (e->type == HPX_OPENCL);
}

#endif // LIBHPX_LIBHPX_ACTIONS_TABLE_H
