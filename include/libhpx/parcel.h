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
#ifndef LIBHPX_PARCEL_H
#define LIBHPX_PARCEL_H

#include <hpx/hpx.h>
#include "libhpx/instrumentation.h"
#include "libhpx/instrumentation_events.h"

struct ustack;

/// The hpx_parcel structure is what the user-level interacts with.
///
/// @field       ustack A pointer to a stack.
/// @field         next A pointer to the next parcel.
/// @field          src The src rank for the parcel.
/// @field         size The data size in bytes.
/// @field       action The target action identifier.
/// @field       target The target address for parcel_send().
/// @field     c_action The continuation action identifier.
/// @field     c_target The target address for the continuation.
/// @field       buffer Either an in-place payload, or a pointer.
typedef struct {
  uint16_t inplace:1;
  uint16_t        :15;
} parcel_state_t;

// Verify that this bitfield is actually being packed correctly.
_HPX_ASSERT(sizeof(parcel_state_t) == 2, packed_parcel_state);

struct hpx_parcel {
  struct ustack   *ustack;
  struct hpx_parcel *next;
  int                 src;
  uint32_t           size;
  parcel_state_t    state;
  uint16_t         offset;
  hpx_action_t     action;
  hpx_action_t   c_action;
  hpx_addr_t       target;
  hpx_addr_t     c_target;
  hpx_pid_t           pid;
  uint64_t         credit;
#ifdef ENABLE_INSTRUMENTATION
  uint64_t             id;
#endif
  char           buffer[];
};

// Verify an assumption about how big the parcel structure is.
#ifdef ENABLE_INSTRUMENTATION
_HPX_ASSERT(sizeof(hpx_parcel_t) == 72, parcel_size);
#else
_HPX_ASSERT(sizeof(hpx_parcel_t) == HPX_CACHELINE_SIZE, parcel_size);
#endif

/// Parcel tracing events.
/// @{
static inline void INST_EVENT_PARCEL_CREATE(hpx_parcel_t *p) {
  static const int class = HPX_INST_CLASS_PARCEL;
  static const int id = HPX_INST_EVENT_PARCEL_CREATE;
  inst_trace(class, id, p->id, p->action, p->size);
}

static inline void INST_EVENT_PARCEL_SEND(hpx_parcel_t *p) {
  static const int class = HPX_INST_CLASS_PARCEL;
  static const int id = HPX_INST_EVENT_PARCEL_SEND;
  inst_trace(class, id, p->id, p->action, p->size, p->target);
}

static inline void INST_EVENT_PARCEL_RECV(hpx_parcel_t *p) {
  static const int class = HPX_INST_CLASS_PARCEL;
  static const int id = HPX_INST_EVENT_PARCEL_RECV;
  inst_trace(class, id, p->id, p->action, p->size, p->src);
}

static inline void INST_EVENT_PARCEL_RUN(hpx_parcel_t *p) {
  static const int class = HPX_INST_CLASS_PARCEL;
  static const int id = HPX_INST_EVENT_PARCEL_RUN;
  inst_trace(class, id, p->id, p->action, p->size);
}

static inline void INST_EVENT_PARCEL_END(hpx_parcel_t *p) {
  static const int class = HPX_INST_CLASS_PARCEL;
  static const int id = HPX_INST_EVENT_PARCEL_END;
  inst_trace(class, id, p->id, p->action);
}
/// @}

hpx_parcel_t *parcel_create(hpx_addr_t addr, hpx_action_t action,
                            const void *args, size_t len, hpx_addr_t c_target,
                            hpx_action_t c_action, hpx_pid_t pid, bool inplace)
  HPX_INTERNAL;

struct ustack *parcel_set_stack(hpx_parcel_t *p, struct ustack *stack)
  HPX_NON_NULL(1) HPX_INTERNAL;

struct ustack *parcel_get_stack(const hpx_parcel_t *p)
  HPX_NON_NULL(1) HPX_INTERNAL;

/// The core send operation.
///
/// This sends the parcel synchronously. This assumes that the parcel has been
/// serialized and has credit already, if necessary.
int parcel_launch(hpx_parcel_t *p)
  HPX_NON_NULL(1);

/// Treat a parcel as a stack of parcels, and pop the top.
///
/// @param[in,out] stack The address of the top parcel in the stack, modified
///                      as a side effect of the call.
///
/// @returns            NULL, or the parcel that was on top of the stack.
hpx_parcel_t *parcel_stack_pop(hpx_parcel_t **stack)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Treat a parcel as a stack of parcels, and push the parcel.
///
/// @param[in,out] stack The address of the top parcel in the stack, modified
///                      as a side effect of the call.
/// @param[in]    parcel The new top of the stack.
void parcel_stack_push(hpx_parcel_t **stack, hpx_parcel_t *parcel)
  HPX_INTERNAL HPX_NON_NULL(1, 2);


static inline uint32_t parcel_size(const hpx_parcel_t *p) {
  return sizeof(*p) + p->size;
}

static inline uint32_t parcel_payload_size(const hpx_parcel_t *p) {
  return p->size;
}

#endif // LIBHPX_PARCEL_H
