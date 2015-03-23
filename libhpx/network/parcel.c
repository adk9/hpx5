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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/// @brief The parcel layer.
///
/// Parcels are the foundation of HPX. The parcel structure serves as both the
/// actual, "on-the-wire," network data structure, as well as the
/// "thread-control-block" descriptor for the threading subsystem.
#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <ffi.h>
#include <hpx/hpx.h>
#include <libsync/sync.h>
#include <libhpx/action.h>
#include <libhpx/attach.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/instrumentation.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>

#ifdef ENABLE_INSTRUMENTATION
static uint64_t parcel_count = 0;
#endif

static size_t _max(size_t lhs, size_t rhs) {
  return (lhs > rhs) ? lhs : rhs;
}

static int _delete_launch_through_parcel_handler(hpx_parcel_t *p) {
  hpx_addr_t lsync = hpx_thread_current_target();
  hpx_lco_wait(lsync);
  parcel_delete(p);
  return HPX_SUCCESS;
}
static HPX_ACTION_DEF(DEFAULT, _delete_launch_through_parcel_handler,
                      _delete_launch_through_parcel, HPX_POINTER);

static void _prepare(hpx_parcel_t *p) {
  parcel_state_t state = parcel_get_state(p);
  if (!state.serialized && p->size) {
    void *buffer = hpx_parcel_get_data(p);
    memcpy(&p->buffer, buffer, p->size);
    state.serialized = 1;
    parcel_set_state(p, state);
  }

  if (p->pid && !p->credit) {
    hpx_parcel_t *parent = scheduler_current_parcel();
    dbg_assert(parent->pid == p->pid);
    p->credit = ++parent->credit;
  }
}

void parcel_set_state(hpx_parcel_t *p, parcel_state_t state) {
  atomic_state_t atomic = {
    .flags = state
  };
  sync_store(&p->state.bits, atomic.bits, SYNC_RELEASE);
}

parcel_state_t parcel_get_state(const hpx_parcel_t *p) {
  atomic_state_t atomic = {
    .bits = sync_load(&p->state.bits, SYNC_ACQUIRE)
  };
  return atomic.flags;
}

parcel_state_t parcel_exchange_state(hpx_parcel_t *p, parcel_state_t state) {
  atomic_state_t atomic = {
    .flags = state
  };
  atomic.bits = sync_swap(&p->state.bits, atomic.bits, SYNC_ACQ_REL);
  return atomic.flags;
}

int parcel_launch(hpx_parcel_t *p) {
  dbg_assert(p->action);

  _prepare(p);

  log_parcel("PID:%"PRIu64" CREDIT:%"PRIu64" %s(%p,%u)@(%"PRIu64") => %s@(%"PRIu64")\n",
             p->pid,
             p->credit,
             action_table_get_key(here->actions, p->action),
             hpx_parcel_get_data(p),
             p->size,
             p->target,
             action_table_get_key(here->actions, p->c_action),
             p->c_target);

  // do a local send through loopback, bypassing the network, otherwise dump the
  // parcel out to the network
  if (gas_owner_of(here->gas, p->target) == here->rank) {
    scheduler_spawn(p);
    return HPX_SUCCESS;
  }
  else {
    int e = network_send(here->network, p);
    dbg_check(e, "failed to perform a network send\n");
    return e;
  }
}

int parcel_launch_through(hpx_parcel_t *p, hpx_addr_t gate) {
  dbg_assert(p->action);
  _prepare(p);

  hpx_parcel_t *pattach = parcel_new(gate, attach, 0, 0,
                                     hpx_thread_current_pid(), p,
                                     parcel_size(p));
  return parcel_launch(pattach);
}

void parcel_init(hpx_addr_t target, hpx_action_t action, hpx_addr_t c_target,
                 hpx_action_t c_action, hpx_pid_t pid, const void *data,
                 size_t len, hpx_parcel_t *p)
{
  static const parcel_state_t serialized = {
    .serialized = 1,
    .retain = 0,
    .nested = 0,
    .block_allocated = 0
  };

  static const parcel_state_t unserialized = {
    .serialized = 0,
    .retain = 0,
    .nested = 0,
    .block_allocated = 0
  };

  p->ustack   = NULL;
  p->next     = NULL;
  p->src      = here->rank;
  p->size     = len;
  p->offset   = 0;
  p->action   = action;
  p->c_action = c_action;
  p->target   = target;
  p->c_target = c_target;
  p->pid      = pid;
  p->credit   = 0;

#ifdef ENABLE_INSTRUMENTATION
  if (config_trace_classes_isset(here->config, HPX_INST_CLASS_PARCEL)) {
    parcel_count = sync_fadd(&parcel_count, 1, SYNC_RELAXED);
    p->id = ((uint64_t)(0xfffff && hpx_get_my_rank()) << 40) | parcel_count;
  }
#endif

  // If there's a user-defined buffer, then remember it---we'll serialize it
  // later, during the send operation.
  if (data && len) {
    // use memcpy to preserve strict aliasing
    memcpy(&p->buffer, &data, sizeof(data));
    parcel_set_state(p, unserialized);
  }
  else {
    parcel_set_state(p, serialized);
  }
}

hpx_parcel_t *parcel_new(hpx_addr_t target, hpx_action_t action,
                         hpx_addr_t c_target, hpx_action_t c_action,
                         hpx_pid_t pid, const void *data, size_t len) {
  size_t size = sizeof(hpx_parcel_t);
  if (len != 0) {
    size += _max(sizeof(void*), len);
  }

  hpx_parcel_t *p = registered_memalign(HPX_CACHELINE_SIZE, size);
  dbg_assert_str(p, "parcel: failed to allocate %zu registered bytes.\n", size);
  parcel_init(target, action, c_target, c_action, pid, data, len, p);
  INST_EVENT_PARCEL_CREATE(p);
  return p;
}

hpx_parcel_t *parcel_clone(const hpx_parcel_t *p) {
  static const parcel_state_t state = {
    .serialized = 1,
    .retain = 0,
    .nested = 0,
    .block_allocated = 0
  };

  dbg_assert(parcel_get_state(p).serialized || p->size == 0);
  size_t n = parcel_size(p);
  hpx_parcel_t *clone = registered_memalign(HPX_CACHELINE_SIZE, n);
  memcpy(clone, p, n);
  clone->ustack = NULL;
  clone->next = NULL;
  parcel_set_state(clone, state);
  return clone;
}

void parcel_delete(hpx_parcel_t *p) {
  if (!p) {
    return;
  }

  parcel_state_t state = parcel_get_state(p);

  if (state.nested) {
    size_t n = parcel_size(p);
    hpx_parcel_t *parent = (void*)((char*)p - sizeof(*p));
    size_t m = parent->size;
    if (n != m) {
      dbg_error("expected payload of %zu for parent, saw %zu\n", n, m);
    }
    parcel_delete(parent);
    return;
  }

  if (state.retain) {
    state.retain = 0;
    state = parcel_exchange_state(p, state);
  }

  if (!state.retain) {
    registered_free(p);
  }
}

struct ustack* parcel_set_stack(hpx_parcel_t *p, struct ustack *next) {
  assert((uintptr_t)next % sizeof(void*) == 0);
  // This can detect races in the scheduler when two threads try and process the
  // same parcel.
  if (DEBUG) {
    return sync_swap(&p->ustack, next, SYNC_ACQ_REL);
  }
  else {
    p->ustack = next;
    return NULL;
  }
}

struct ustack *parcel_get_stack(const hpx_parcel_t *p) {
  return p->ustack;
}
