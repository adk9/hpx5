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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// @brief The parcel layer.
///
/// Parcels are the foundation of HPX. The parcel structure serves as both the
/// actual, "on-the-wire," network data structure, as well as the
/// "thread-control-block" descriptor for the threading subsystem.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "hpx/hpx.h"

#include "libhpx/action.h"
#include "libhpx/btt.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "padding.h"

static const uintptr_t _INPLACE_MASK = 0x1;
static const uintptr_t   _STATE_MASK = 0x1;
static const size_t _SMALL_THRESHOLD = HPX_PAGE_SIZE;
//PAD_TO_CACHELINE(sizeof(hpx_parcel_t)) +
//                                     HPX_CACHELINE_SIZE;

static size_t _max(size_t lhs, size_t rhs) {
  return (lhs > rhs) ? lhs : rhs;
}

void hpx_parcel_set_action(hpx_parcel_t *p, const hpx_action_t action) {
  p->action = action;
}


void hpx_parcel_set_target(hpx_parcel_t *p, const hpx_addr_t addr) {
  p->target = addr;
}

void hpx_parcel_set_cont_action(hpx_parcel_t *p, const hpx_action_t action) {
  p->c_action = action;
}

void hpx_parcel_set_cont_target(hpx_parcel_t *p, const hpx_addr_t cont) {
  p->c_target = cont;
}


void hpx_parcel_set_data(hpx_parcel_t *p, const void *data, int size) {
  if (size) {
    void *to = hpx_parcel_get_data(p);
    memcpy(to, data, size);
  }
}


hpx_action_t hpx_parcel_get_action(const hpx_parcel_t *p) {
  return p->action;
}


hpx_addr_t hpx_parcel_get_target(const hpx_parcel_t *p) {
  return p->target;
}


hpx_action_t hpx_parcel_get_cont_action(const hpx_parcel_t *p) {
  return p->c_action;
}


hpx_addr_t hpx_parcel_get_cont_target(const hpx_parcel_t *p) {
  return p->c_target;
}


void *hpx_parcel_get_data(hpx_parcel_t *p) {
  if (p->size == 0)
    return NULL;

  void *buffer = NULL;
  uintptr_t inplace = ((uintptr_t)p->ustack & _INPLACE_MASK);
  if (inplace)
    buffer = &p->buffer;
  else
    memcpy(&buffer, &p->buffer, sizeof(buffer));
  return buffer;
}


// ----------------------------------------------------------------------------
/// Acquire a parcel structure.
///
/// Parcels are always acquired with enough inline space to support
/// serialization of @p bytes bytes. If @p buffer is NULL, then the parcel is
/// marked as inplace, meaning that the parcel's buffer is being used directly
/// (or not at all if @p bytes is 0). If the @p buffer pointer is non-NULL, then
/// the parcel is marked as out-of-place---we will keep the @p buffer pointer in
/// the lowest sizeof(void*) bytes of the parcel's in-place buffer.
///
/// At hpx_parcel_send() or hpx_parcel_send_sync() the runtime will copy an
/// out-of-place buffer into the parcel's inline buffer, either synchronously or
/// asynchronously depending on which interface is used, and how big the buffer
/// is.
// ----------------------------------------------------------------------------
hpx_parcel_t *
hpx_parcel_acquire(const void *buffer, size_t bytes) {
  // figure out how big a parcel buffer I actually need
  size_t size = sizeof(hpx_parcel_t);
  if (bytes != 0)
    size += _max(sizeof(void*), bytes);

  // allocate a parcel with enough space to buffer the @p buffer
  hpx_parcel_t *p = network_malloc(size, HPX_CACHELINE_SIZE);
  if (!p) {
    dbg_error("parcel: failed to get an %lu bytes from the allocator.\n", bytes);
    return NULL;
  }

  // initialize the structure with defaults
  p->ustack   = (struct ustack*)_INPLACE_MASK;
  p->src      = here->rank;
  p->size     = bytes;
  p->action   = HPX_ACTION_NULL;
  p->target   = HPX_HERE;
  p->c_action = HPX_ACTION_NULL;
  p->c_target = HPX_NULL;

  // if there's a user-defined buffer, then remember it
  if (buffer) {
    p->ustack = NULL;
    memcpy(&p->buffer, &buffer, sizeof(buffer));
  }

  return p;
}


// ----------------------------------------------------------------------------
/// Perform an asynchronous send operation.
///
/// Simply wraps the send operation in an asynchronous interface.
///
/// @param   p the parcel to send (may need serialization)
/// @continues NULL
/// @returns   HPX_SUCCESS
// ----------------------------------------------------------------------------
static hpx_action_t _send_async = 0;


static int _send_async_action(hpx_parcel_t **p) {
  hpx_parcel_send_sync(*p);
  return HPX_SUCCESS;
}


static HPX_CONSTRUCTOR void _init_actions(void) {
  _send_async = HPX_REGISTER_ACTION(_send_async_action);
}


void
hpx_parcel_send(hpx_parcel_t *p, hpx_addr_t lsync) {
  bool inplace = ((uintptr_t)p->ustack & _INPLACE_MASK);
  bool small = p->size < _SMALL_THRESHOLD;

  // do a true async send, if we should
  if (!inplace && !small) {
    hpx_call(HPX_HERE, _send_async, &p, sizeof(p), lsync);
    return;
  }

  // otherwise, do a synchronous send and set the lsync LCO, if there is one
  hpx_parcel_send_sync(p);
  if (!hpx_addr_eq(lsync, HPX_NULL))
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
}


void
hpx_parcel_send_sync(hpx_parcel_t *p) {
  // an out-of-place parcel always needs to be serialized, either for a local or
  // remote send---parcels are always allocated with a serialization buffer for
  // this purpose
  bool inplace = ((uintptr_t)p->ustack & _INPLACE_MASK);
  if (!inplace && p->size) {
    void *buffer = hpx_parcel_get_data(p);
    memcpy(&p->buffer, buffer, p->size);
    p->ustack = (struct ustack*)((uintptr_t)p->ustack | _INPLACE_MASK);
  }

  dbg_log_parcel("parcel: %s(%p,%u)@(%lu,%u,%u) => %s@(%lu,%u,%u).\n",
                 action_get_key(p->action), hpx_parcel_get_data(p),
                 p->size, p->target.offset, p->target.base_id,
                 p->target.block_bytes, action_get_key(p->c_action),
                 p->c_target.offset, p->c_target.base_id,
                 p->c_target.block_bytes);
  
  // do a local send through loopback
  bool local = (btt_owner(here->btt, p->target) == here->rank);
  if (local) {
    scheduler_spawn(p);
    return;
  }

  // do a network send
  network_tx_enqueue(here->network, p);
}


void
hpx_parcel_release(hpx_parcel_t *p) {
  network_free(p);
}


hpx_parcel_t *
parcel_create(hpx_addr_t target, hpx_action_t action, const void *args,
              size_t len, hpx_addr_t c_target, hpx_action_t c_action,
              bool inplace)
{
  hpx_parcel_t *p = hpx_parcel_acquire(inplace ? NULL : args, len);
  if (!p) {
    dbg_error("parcel: could not allocate parcel.\n");
    return NULL;
  }

  hpx_parcel_set_action(p, action);
  hpx_parcel_set_target(p, target);
  hpx_parcel_set_cont_action(p, c_action);
  hpx_parcel_set_cont_target(p, c_target);
  if (inplace)
    hpx_parcel_set_data(p, args, len);
  return p;
}

void
parcel_set_stack(hpx_parcel_t *p, struct ustack *stack) {
  assert((uintptr_t)stack % sizeof(void*) == 0);
  uintptr_t state = (uintptr_t)p->ustack & _STATE_MASK;
  p->ustack = (struct ustack*)(state | (uintptr_t)stack);
}

struct ustack *
parcel_get_stack(hpx_parcel_t *p) {
  return (struct ustack*)((uintptr_t)p->ustack & ~_STATE_MASK);
}
