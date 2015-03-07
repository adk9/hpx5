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
#include "config.h"
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
#include <jemalloc/jemalloc_global.h>
#include <ffi.h>
#include <hpx/hpx.h>
#include <libsync/sync.h>
#include <libhpx/action.h>
#include <libhpx/attach.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/gas.h>
#include <libhpx/locality.h>
#include <libhpx/instrumentation.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include "padding.h"

static const uintptr_t _INPLACE_MASK = 0x1;
static const uintptr_t   _STATE_MASK = 0x1;
static const size_t _SMALL_THRESHOLD = HPX_PAGE_SIZE;
#ifdef ENABLE_INSTRUMENTATION
static uint64_t parcel_count = 0;
#endif

static size_t _max(size_t lhs, size_t rhs) {
  return (lhs > rhs) ? lhs : rhs;
}

static uintptr_t _inplace(const hpx_parcel_t *p) {
  return ((uintptr_t)p->ustack & _INPLACE_MASK);
}

static int _blessed(const hpx_parcel_t *p) {
  return (!p->pid || p->credit);
}

static void _serialize(hpx_parcel_t *p) {
  if (!p->size)
    return;

  if (_inplace(p))
    return;

  void *buffer = hpx_parcel_get_data(p);
  memcpy(&p->buffer, buffer, p->size);
  p->ustack = (struct ustack*)((uintptr_t)p->ustack | _INPLACE_MASK);
}

static void _bless(hpx_parcel_t *p) {
  hpx_pid_t pid = p->pid;
  if (!pid)
    return;

  uint64_t credit = p->credit;
  if (!credit) {
    // split the parent's current credit. the parent retains half..
    hpx_parcel_t *parent = scheduler_current_parcel();
    dbg_assert_str(parent, "no parent to bless child parcel\n");
    // parent and child each get half a credit
    p->credit = ++parent->credit;
  }
}

static void _prepare(hpx_parcel_t *p) {
  _serialize(p);
  _bless(p);
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
    memmove(to, data, size);
  }
}

void _hpx_parcel_set_args(hpx_parcel_t *p, int nargs, ...) {
  if (p->action == HPX_ACTION_NULL) {
    dbg_error("parcel must have an action to serialize arguments to its buffer.\n");
  }

  ffi_cif *cif = action_table_get_cif(here->actions, p->action);
  if (!cif) {
    dbg_error("parcel must have an action to serialize arguments to its buffer.\n");
  }
  else if (nargs != cif->nargs) {
    dbg_error("expecting %d arguments for action %s (%d given).\n",
              cif->nargs, action_table_get_key(here->actions, p->action), nargs);
  }

  void *argps[nargs];
  va_list vargs;
  va_start(vargs, nargs);
  for (int i = 0; i < nargs; ++i) {
    argps[i] = va_arg(vargs, void*);
  }
  va_end(vargs);

  size_t len = ffi_raw_size(cif);
  dbg_assert(len > 0);

  if (len) {
    ffi_raw *to = hpx_parcel_get_data(p);
    ffi_ptrarray_to_raw(cif, argps, to);
  }
  return;
}


void hpx_parcel_set_pid(hpx_parcel_t *p, const hpx_pid_t pid) {
  p->pid = pid;
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
  void *buffer = NULL;
  if (p->size == 0) {
    return buffer;
  }

  if (_inplace(p)) {
    buffer = &p->buffer;
  }
  else {
    memcpy(&buffer, &p->buffer, sizeof(buffer));
  }

  return buffer;
}

hpx_pid_t hpx_parcel_get_pid(const hpx_parcel_t *p) {
  return p->pid;
}

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
hpx_parcel_t *hpx_parcel_acquire(const void *buffer, size_t bytes) {
  // figure out how big a parcel buffer I actually need
  size_t size = sizeof(hpx_parcel_t);
  if (bytes != 0) {
    size += _max(sizeof(void*), bytes);
  }

  // allocate a parcel with enough space to buffer the @p buffer
  hpx_parcel_t *p = libhpx_global_memalign(HPX_CACHELINE_SIZE, size);

  if (!p) {
    dbg_error("parcel: failed to get an %zu bytes from the allocator.\n", bytes);
    return NULL;
  }

  // initialize the structure with defaults
  p->ustack   = (struct ustack*)_INPLACE_MASK;
  p->next     = NULL;
  p->pid      = hpx_thread_current_pid();
  p->src      = here->rank;
  p->size     = bytes;
  p->action   = HPX_ACTION_NULL;
  p->target   = HPX_HERE;
  p->c_action = HPX_ACTION_NULL;
  p->c_target = HPX_NULL;
  p->credit   = 0;
#ifdef ENABLE_INSTRUMENTATION
  if (config_trace_classes_isset(here->config, HPX_INST_CLASS_PARCEL)) {
    parcel_count = sync_fadd(&parcel_count, 1, SYNC_RELAXED);
    p->id = ((uint64_t)(0xfffff && hpx_get_my_rank()) << 40) | parcel_count;
  }
#endif

  // If there's a user-defined buffer, then remember it---we'll serialize it
  // later, during the send operation. We occasionally see a buffer without
  // bytes so skip in that context too.
  if (buffer && bytes) {
    p->ustack = NULL;
    memcpy(&p->buffer, &buffer, sizeof(buffer));
  }

  INST_EVENT_PARCEL_CREATE(p);

  return p;
}

/// Perform an asynchronous send operation.
static HPX_ACTION(_parcel_send_async, hpx_parcel_t **p) {
  int e = hpx_parcel_send_sync(*p);
  dbg_check(e, "failed to send a parcel\n");
  return HPX_SUCCESS;
}

int parcel_launch(hpx_parcel_t *p) {
  dbg_assert(_inplace(p));
  dbg_assert(_blessed(p));

  // LOG
  if (p->c_action != HPX_ACTION_NULL) {
    log_parcel("PID:%"PRIu64" CREDIT:%"PRIu64" %s(%p,%u)@(%"PRIu64") => %s@(%"PRIu64")\n",
                   p->pid,
                   p->credit,
                   action_table_get_key(here->actions, p->action),
                   hpx_parcel_get_data(p),
                   p->size,
                   p->target,
                   action_table_get_key(here->actions, p->c_action),
                   p->c_target);
  } else {
    log_parcel("PID:%"PRIu64" CREDIT:%"PRIu64" %s(%p,%u)@(%"PRIu64")\n",
                   p->pid,
                   p->credit,
                   action_table_get_key(here->actions, p->action),
                   hpx_parcel_get_data(p),
                   p->size,
                   p->target);
  }

  // do a local send through loopback, bypassing the network, otherwise dump the
  // parcel out to the network
  if (gas_owner_of(here->gas, p->target) == here->rank) {
    scheduler_spawn(p);
    return HPX_SUCCESS;
  }

  // do a network send
  int e = network_send(here->network, p);
  dbg_check(e, "failed to perform a network send\n");
  return HPX_SUCCESS;
}

hpx_status_t hpx_parcel_send_sync(hpx_parcel_t *p) {
  INST_EVENT_PARCEL_SEND(p);
  _prepare(p);
  return parcel_launch(p);
}

hpx_status_t hpx_parcel_send(hpx_parcel_t *p, hpx_addr_t lsync) {
  if (p->size >= _SMALL_THRESHOLD && !_inplace(p)) {
    return hpx_call(HPX_HERE, _parcel_send_async, lsync, &p, sizeof(p));
  }

  _prepare(p);
  hpx_status_t status = parcel_launch(p);
  hpx_lco_error(lsync, status, HPX_NULL);
  return status;
}

hpx_status_t hpx_parcel_send_through_sync(hpx_parcel_t *p, hpx_addr_t gate) {
  _prepare(p);
  dbg_assert(p->target != HPX_NULL);
  return hpx_call(gate, attach, HPX_NULL, p, parcel_size(p));
}

hpx_status_t hpx_parcel_send_through(hpx_parcel_t *p, hpx_addr_t gate,
                                     hpx_addr_t lsync) {
  _prepare(p);
  dbg_assert(p->target != HPX_NULL);
  return hpx_call_async(gate, attach, lsync, HPX_NULL, p, parcel_size(p));
}

void hpx_parcel_release(hpx_parcel_t *p) {
  libhpx_global_free(p);
}

hpx_parcel_t *parcel_create(hpx_addr_t target, hpx_action_t action,
                            const void *args, size_t len, hpx_addr_t c_target,
                            hpx_action_t c_action, hpx_pid_t pid, bool inplace)
{
  hpx_parcel_t *p = hpx_parcel_acquire(inplace ? NULL : args, len);
  if (!p) {
    dbg_error("parcel: could not allocate parcel.\n");
    return NULL;
  }

  hpx_parcel_set_pid(p, pid);
  parcel_set_credit(p, 0);
  hpx_parcel_set_action(p, action);
  hpx_parcel_set_target(p, target);
  hpx_parcel_set_cont_action(p, c_action);
  hpx_parcel_set_cont_target(p, c_target);
  if (inplace)
    hpx_parcel_set_data(p, args, len);
  return p;
}

struct ustack* parcel_set_stack(hpx_parcel_t *p, struct ustack *stack) {
  assert((uintptr_t)stack % sizeof(void*) == 0);
  uintptr_t state = (uintptr_t)p->ustack & _STATE_MASK;
  struct ustack *next = (struct ustack*)(state | (uintptr_t)stack);
  // This can detect races in the scheduler when two threads try and process the
  // same parcel.
  if (DEBUG) {
    struct ustack *prev = sync_swap(&p->ustack, next, SYNC_ACQ_REL);
    return (void*)((uintptr_t)prev & ~_STATE_MASK);
  }
  else {
    p->ustack = next;
    return NULL;
  }
}

struct ustack *parcel_get_stack(const hpx_parcel_t *p) {
  return (struct ustack*)((uintptr_t)p->ustack & ~_STATE_MASK);
}

void parcel_set_credit(hpx_parcel_t *p, const uint64_t credit) {
  p->credit = credit;
}

uint64_t parcel_get_credit(const hpx_parcel_t *p) {
  return p->credit;
}

hpx_parcel_t *parcel_stack_pop(hpx_parcel_t **stack) {
  hpx_parcel_t *top = *stack;
  if (top) {
    *stack = top->next;
    top->next = NULL;
  }
  return top;
}

void parcel_stack_push(hpx_parcel_t **stack, hpx_parcel_t *parcel) {
  DEBUG_IF (parcel->next != NULL) {
    dbg_error("parcel should not have an active next during push.\n");
  }
  parcel->next = *stack;
  *stack = parcel;
}

void parcel_stack_foreach(hpx_parcel_t *p, void *env,
                         void (*f)(hpx_parcel_t*, void*))
{
  while (p) {
    f(p, env);
    p = (void*)parcel_get_stack(p);
  }
}
