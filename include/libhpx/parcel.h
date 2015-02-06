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
#ifndef LIBHPX_PARCEL_H
#define LIBHPX_PARCEL_H

#include "hpx/hpx.h"
#include "libsync/sync.h"

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
struct hpx_parcel {
  struct ustack   *ustack;
  struct hpx_parcel *next;
  int                 src;
  uint32_t           size;
  hpx_action_t     action;
  hpx_addr_t       target;
  hpx_action_t   c_action;
  hpx_addr_t     c_target;
  hpx_pid_t           pid;
  uint64_t         credit;
#ifdef ENABLE_INSTRUMENTATION
  uint64_t             id;
#endif
  char           buffer[];
};


typedef struct parcel_queue {
  hpx_parcel_t *head;
  hpx_parcel_t *tail;
} parcel_queue_t;


hpx_parcel_t *parcel_create(hpx_addr_t addr, hpx_action_t action,
                            const void *args, size_t len, hpx_addr_t c_target,
                            hpx_action_t c_action, hpx_pid_t pid, bool inplace)
  HPX_INTERNAL;

void parcel_set_stack(hpx_parcel_t *p, struct ustack *stack)
  HPX_NON_NULL(1) HPX_INTERNAL;

struct ustack *parcel_get_stack(const hpx_parcel_t *p)
  HPX_NON_NULL(1) HPX_INTERNAL;

void parcel_set_credit(hpx_parcel_t *p, const uint64_t credit)
  HPX_NON_NULL(1) HPX_INTERNAL;

uint64_t parcel_get_credit(const hpx_parcel_t *p)
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

/// Scan through a parcel list applying the passed function to each one.
void parcel_stack_foreach(hpx_parcel_t *p, void *env,
                          void (*f)(hpx_parcel_t*, void*))
  HPX_INTERNAL HPX_NON_NULL(3);


void parcel_queue_init(parcel_queue_t *q)
  HPX_INTERNAL HPX_NON_NULL(1);


void parcel_queue_fini(parcel_queue_t *q)
  HPX_INTERNAL HPX_NON_NULL(1);


void parcel_queue_enqueue(parcel_queue_t *q, hpx_parcel_t *p)
  HPX_INTERNAL HPX_NON_NULL(1, 2);


hpx_parcel_t *parcel_queue_dequeue(parcel_queue_t *q)
  HPX_INTERNAL HPX_NON_NULL(1);


hpx_parcel_t *parcel_queue_dequeue_all(parcel_queue_t *q)
  HPX_INTERNAL HPX_NON_NULL(1);


static inline uint32_t parcel_size(const hpx_parcel_t *p) {
  return sizeof(*p) + p->size;
}

static inline uint32_t parcel_payload_size(const hpx_parcel_t *p) {
  return p->size;
}

#endif // LIBHPX_PARCEL_H
