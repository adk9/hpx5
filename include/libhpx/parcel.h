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
#include <libhpx/instrumentation.h>
#include <libhpx/instrumentation_events.h>

struct ustack;

typedef struct {
  uint16_t      serialized :1;
  uint16_t          retain :1;
  uint16_t          nested :1;
  uint16_t block_allocated :1;
  uint16_t                 :12;
} parcel_state_t;

typedef union {
  parcel_state_t flags;
  uint16_t bits;
} atomic_state_t;

// Verify that this bitfield is actually being packed correctly.
_HPX_ASSERT(sizeof(parcel_state_t) == 2, packed_parcel_state);

/// The hpx_parcel structure is what the user-level interacts with.
///
/// @field       ustack A pointer to a stack.
/// @field         next A pointer to the next parcel.
/// @field          src The src rank for the parcel.
/// @field         size The data size in bytes.
/// @field        state The parcel's state bits.
/// @field       offset Reserved for future use.
/// @field       action The target action identifier.
/// @field     c_action The continuation action identifier.
/// @field       target The target address for parcel_send().
/// @field     c_target The target address for the continuation.
/// @field           id A unique identifier for parcel tracing.
/// @field       buffer Either an in-place payload, or a pointer.
struct hpx_parcel {
  struct ustack   *ustack;
  struct hpx_parcel *next;
  int                 src;
  uint32_t           size;
  atomic_state_t    state;
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
  char             buffer[];
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

void parcel_init(hpx_addr_t target, hpx_action_t action, hpx_addr_t c_target,
                 hpx_action_t c_action, hpx_pid_t pid, const void *data,
                 size_t len, hpx_parcel_t *p)
  HPX_INTERNAL;

hpx_parcel_t *parcel_new(hpx_addr_t target, hpx_action_t action, hpx_addr_t c_target,
                         hpx_action_t c_action, hpx_pid_t pid, const void *data,
                         size_t len)
  HPX_INTERNAL;

hpx_parcel_t *parcel_clone(const hpx_parcel_t *p)
  HPX_INTERNAL;

void parcel_delete(hpx_parcel_t *p)
  HPX_INTERNAL;

struct ustack *parcel_set_stack(hpx_parcel_t *p, struct ustack *stack)
  HPX_NON_NULL(1) HPX_INTERNAL;

struct ustack *parcel_get_stack(const hpx_parcel_t *p)
  HPX_NON_NULL(1) HPX_INTERNAL;

/// The core send operation.
///
/// This sends the parcel synchronously. This will eagerly serialize the parcel,
/// and will assign it credit from the currently executing process if it has a
/// pid set.
int parcel_launch(hpx_parcel_t *p)
  HPX_INTERNAL;

int parcel_launch_through(hpx_parcel_t *p, hpx_addr_t gate)
  HPX_INTERNAL;

void parcel_set_state(hpx_parcel_t *p, parcel_state_t state)
  HPX_INTERNAL;

parcel_state_t parcel_get_state(const hpx_parcel_t *p)
  HPX_INTERNAL;

parcel_state_t parcel_exchange_state(hpx_parcel_t *p, parcel_state_t state)
  HPX_INTERNAL;

/// Treat a parcel as a stack of parcels, and pop the top.
///
/// @param[in,out] stack The address of the top parcel in the stack, modified
///                      as a side effect of the call.
///
/// @returns            NULL, or the parcel that was on top of the stack.
static inline hpx_parcel_t *parcel_stack_pop(hpx_parcel_t **stack) {
  hpx_parcel_t *top = *stack;
  if (top) {
    *stack = top->next;
    top->next = NULL;
  }
  return top;
}

/// Treat a parcel as a stack of parcels, and push the parcel.
///
/// @param[in,out] stack The address of the top parcel in the stack, modified
///                      as a side effect of the call.
/// @param[in]    parcel The new top of the stack.
static inline void parcel_stack_push(hpx_parcel_t **stack, hpx_parcel_t *p) {
  p->next = *stack;
  *stack = p;
}

static inline uint32_t parcel_size(const hpx_parcel_t *p) {
  return sizeof(*p) + p->size;
}

static inline uint32_t parcel_payload_size(const hpx_parcel_t *p) {
  return p->size;
}

#endif // LIBHPX_PARCEL_H
