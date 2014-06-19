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
/// @file libhpx/scheduler/process.c
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "libsync/sync.h"
#include "libhpx/debug.h"
#include "libhpx/action.h"
#include "libhpx/btt.h"
#include "libhpx/locality.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "termination.h"


typedef struct {
  SYNC_ATOMIC(uint32_t) credit;           // credit balance
  bitmap_t               *debt;           // the credit that was recovered 
  hpx_addr_t       termination;           // the termination LCO
} _process_t;


/// Remote action interface to a process.
static hpx_action_t         _delete;
static hpx_action_t _debt_collector;

static bool _is_tracked(_process_t *p) {
  return (!hpx_addr_eq(p->termination, HPX_NULL));
}

/// Remote action to delete a process.
static void _free(_process_t *p) {
  if (!p)
    return;

  cr_bitmap_delete(p->debt);

  // set the termination LCO if the process is being deleted
  if (_is_tracked(p))
    hpx_lco_set(p->termination, 0, NULL, HPX_NULL, HPX_NULL);

  // TODO: hpx_gas_free?
}


/// Initialize a process.
static void _init(_process_t *p, hpx_addr_t termination) {
  sync_store(&p->credit, 0, SYNC_RELEASE);
  p->debt = cr_bitmap_new();
  assert(p->debt);
  p->termination = termination;
}


static int _delete_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  _process_t *p = NULL;
  if (!hpx_gas_try_pin(target, (void**)&p))
    return HPX_RESEND;

  _free(p);
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


static int _debt_collector_action(uint32_t *args) {
  uint32_t credit = *args;
  hpx_addr_t target = hpx_thread_current_target();
  _process_t *p = NULL;
  if (!hpx_gas_try_pin(target, (void**)&p))
    return HPX_RESEND;

  dbg_log("debt collector recovered credit %d.\n", credit);
  cr_bitmap_add(p->debt, credit);

  if (cr_bitmap_test(p->debt)) {
    dbg_log("detected quiescence. HPX is now terminating...\n");
    assert(_is_tracked(p));
    hpx_lco_set(p->termination, 0, NULL, HPX_NULL, HPX_NULL);
  }

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


int
parcel_recover_credit(hpx_parcel_t *p) {
  if (!p->credit || !p->pid)
    return HPX_SUCCESS;

  hpx_addr_t process = hpx_addr_init(0, p->pid, sizeof(_process_t));  
  hpx_parcel_t *pp =
      parcel_create(process, _debt_collector, &p->credit, sizeof(p->credit),
                    HPX_NULL, HPX_ACTION_NULL, 0, false);
  if (!pp)
    return dbg_error("parcel_recover_credit failed.\n");

  hpx_parcel_send_sync(pp);
  return HPX_SUCCESS;
}


static void HPX_CONSTRUCTOR _initialize_actions(void) {
  _delete         = HPX_REGISTER_ACTION(_delete_action);
  _debt_collector = HPX_REGISTER_ACTION(_debt_collector_action);
}


/// Create a new HPX process.
hpx_addr_t
hpx_process_new(hpx_addr_t termination) {
  _process_t *p;
  hpx_addr_t process = hpx_gas_alloc(sizeof(*p));
  if (!hpx_gas_try_pin(process, (void**)&p)) {
    dbg_error("Could not pin newly allocated process.\n");
    hpx_abort();
  }
  _init(p, termination);
  hpx_gas_unpin(process);
  return process;
}


/// Get a process' PID.
hpx_pid_t
hpx_process_getpid(hpx_addr_t process) {
  return (hpx_pid_t)process.base_id;
}


/// ----------------------------------------------------------------------------
/// Call an action in a specified process context.
/// ----------------------------------------------------------------------------
int
hpx_process_call(hpx_addr_t process, hpx_action_t action, const void *args,
                 size_t len, hpx_addr_t result) {
  hpx_pid_t pid = hpx_process_getpid(process);
  hpx_parcel_t *p = parcel_create(process, action, args, len, result,
                                  hpx_lco_set_action, pid, true);
  if (!p)
    return dbg_error("process: failed to create parcel.\n");

  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Deletes a process.
/// ----------------------------------------------------------------------------
void
hpx_process_delete(hpx_addr_t process, hpx_addr_t sync) {
  _process_t *p = NULL;
  if (hpx_gas_try_pin(process, (void**)&p)) {
    _free(p);
    hpx_gas_unpin(process);
    if (!hpx_addr_eq(sync, HPX_NULL))
      hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
    return;
  }

  hpx_call(process, _delete, NULL, 0, sync);
}


int
hpx_process_create(hpx_action_t act, const void *args, size_t size, hpx_addr_t done) {
  hpx_addr_t proc = hpx_process_new(done);
  hpx_pid_t pid = hpx_process_getpid(proc);
  hpx_parcel_t *p = parcel_create(proc, act, args, size, HPX_NULL,
                                  HPX_ACTION_NULL, pid, true);
  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}
