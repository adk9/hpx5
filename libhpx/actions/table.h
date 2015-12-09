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

/// An action table entry type.
///
/// This stores information associated with an HPX action. In
/// particular, an action entry maintains the action handler
/// (function), a globally unique string key, a unique id generated
/// during action finalization, action types and attributes, e.g., can
/// they block, should we pre-pin their arguments, etc., and an
/// environment pointer.
///
typedef struct {
  int           (*execute_parcel)(const void *obj, hpx_parcel_t *buffer);
  hpx_parcel_t *(*new_parcel)(const void *obj, hpx_addr_t addr,
                              hpx_addr_t c_addr, hpx_action_t c_action,
                              int n, va_list *args);
  handler_t      handler;
  hpx_action_t       *id;
  const char        *key;
  hpx_action_type_t type;
  uint32_t          attr;
  ffi_cif           *cif;
  void              *env;
} action_entry_t;

typedef struct action_table {
  int                  n;
  int            padding;
  action_entry_t entries[];
} action_table_t;

void entry_init_execute_parcel(action_entry_t *entry);
void entry_init_new_parcel(action_entry_t *entry);

#ifdef ENABLE_DEBUG
void CHECK_BOUND(const action_table_t *table, hpx_action_t id);
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
