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
#include "libhpx/locality.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "termination.h"

#ifdef ENABLE_TAU
#define TAU_DEFAULT 1
#include <TAU.h>
#endif

typedef struct {
  volatile uint64_t    credit;               // credit balance
  bitmap_t              *debt;               // the credit that was recovered
  hpx_addr_t      termination;               // the termination LCO
} _process_t;


/// Remote action interface to a process.
static hpx_action_t          _call;
static hpx_action_t        _delete;
static hpx_action_t _return_credit;


static bool _is_tracked(_process_t *p) {
  return (p->termination != HPX_NULL);
}

/// Remote action to delete a process.
static void _free(_process_t *p) {
  if (!p)
    return;

  cr_bitmap_delete(p->debt);

  // set the termination LCO if the process is being deleted
  // if (_is_tracked(p))
  //   hpx_lco_set(p->termination, 0, NULL, HPX_NULL, HPX_NULL);
}


/// Initialize a process.
static void _init(_process_t *p, hpx_addr_t termination) {
  sync_store(&p->credit, 0, SYNC_RELEASE);
  p->debt = cr_bitmap_new();
  assert(p->debt);
  p->termination = termination;
}


typedef struct {
  hpx_addr_t result;
  char data[];
} _call_args_t;


static int _call_action(_call_args_t *args) {
  hpx_addr_t process = hpx_thread_current_target();
  _process_t *p = NULL;
  if (!hpx_gas_try_pin(process, (void**)&p))
    return HPX_RESEND;

  uint64_t credit = sync_addf(&p->credit, 1, SYNC_ACQ_REL);
  hpx_gas_unpin(process);

  hpx_pid_t pid = hpx_process_getpid(process);
  hpx_addr_t target = hpx_thread_current_cont_target();
  hpx_action_t action = hpx_thread_current_cont_action();
  uint32_t len = hpx_thread_current_args_size() - sizeof(args->result);
  hpx_parcel_t *parcel =
      parcel_create(target, action, args->data, len, args->result,
                    hpx_lco_set_action, pid, true);
  parcel_set_credit(parcel, credit);
  if (!parcel)
    return dbg_error("process: call_action failed.\n");

  hpx_parcel_send_sync(parcel);
  return HPX_SUCCESS;
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


static int _return_credit_action(uint64_t *args) {
  uint64_t credit = *args;
  hpx_addr_t target = hpx_thread_current_target();
  _process_t *p = NULL;
  if (!hpx_gas_try_pin(target, (void**)&p))
    return HPX_RESEND;

  // add credit to the credit-accounting bitmap
  cr_bitmap_add(p->debt, credit);

  // test for quiescence
  if (cr_bitmap_test(p->debt)) {
    dbg_log("detected quiescence. HPX is now terminating...\n");
    uint64_t total_credit = sync_addf(&p->credit, -1, SYNC_ACQ_REL);
    assert(total_credit == 0);
    assert(_is_tracked(p));
    hpx_lco_set(p->termination, 0, NULL, HPX_NULL, HPX_NULL);
  }

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


int
parcel_recover_credit(hpx_parcel_t *p) {
  hpx_addr_t process = p->pid;
  hpx_parcel_t *pp =
      parcel_create(process, _return_credit, &p->credit, sizeof(p->credit),
                    HPX_NULL, HPX_ACTION_NULL, 0, false);
  if (!pp)
    return dbg_error("parcel_recover_credit failed.\n");

  hpx_parcel_send_sync(pp);
  return HPX_SUCCESS;
}


static void HPX_CONSTRUCTOR _initialize_actions(void) {
  _call          = HPX_REGISTER_ACTION(_call_action);
  _delete        = HPX_REGISTER_ACTION(_delete_action);
  _return_credit = HPX_REGISTER_ACTION(_return_credit_action);
}


/// Create a new HPX process.
hpx_addr_t
hpx_process_new(hpx_addr_t termination) {
#ifdef ENABLE_TAU
          TAU_START("hpx_process_new");
#endif
  if (termination == HPX_NULL)
    return HPX_NULL;
  _process_t *p;
  hpx_addr_t process = hpx_gas_alloc(sizeof(*p));
  if (!hpx_gas_try_pin(process, (void**)&p)) {
    dbg_error("Could not pin newly allocated process.\n");
  }
  _init(p, termination);
  hpx_gas_unpin(process);
#ifdef ENABLE_TAU
          TAU_STOP("hpx_process_new");
#endif
  return process;
}


/// Get a process' PID.
hpx_pid_t
hpx_process_getpid(hpx_addr_t process) {
  return (hpx_pid_t)process;
}


/// ----------------------------------------------------------------------------
/// Call an action in a specified process context.
///
/// When an action is invoked in a process context, the parcel has to
/// request credit from the process owner represented by the address
/// @p process. We set the continuation targets and actions to be the
/// actual action which is to be invoked, and pass the completion
/// continuation as an argument to the _call action.
/// ----------------------------------------------------------------------------
int
hpx_process_call(hpx_addr_t process, hpx_action_t action, const void *args,
                 size_t len, hpx_addr_t result) {
#ifdef ENABLE_TAU
          TAU_START("hpx_process_call");
#endif
  hpx_parcel_t *p;

  if (process == HPX_NULL)
    return HPX_ERROR;

  // do an immediate call if it is an untracked process
  hpx_pid_t pid = hpx_process_getpid(process);
  if (!pid) {
    p = parcel_create(HPX_HERE, action, args, len, result, hpx_lco_set_action, 0, true);
  } else {
    p = hpx_parcel_acquire(NULL, len + sizeof(_call_args_t));
    hpx_parcel_set_target(p, process);
    hpx_parcel_set_action(p, _call);
    hpx_parcel_set_cont_action(p, action);
    hpx_parcel_set_cont_target(p, process);
    hpx_parcel_set_pid(p, 0);

    _call_args_t *call_args = (_call_args_t *)hpx_parcel_get_data(p);
    call_args->result = result;
    memcpy(&call_args->data, args, len);
  }

  hpx_parcel_send_sync(p);
#ifdef ENABLE_TAU
          TAU_STOP("hpx_process_call");
#endif
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Deletes a process.
/// ----------------------------------------------------------------------------
void
hpx_process_delete(hpx_addr_t process, hpx_addr_t sync) {
#ifdef ENABLE_TAU
          TAU_START("hpx_process_delete");
#endif
  if (process == HPX_NULL)
    return;

  hpx_call_sync(process, _delete, NULL, 0, NULL, 0);
  hpx_gas_free(process, sync);

#ifdef ENABLE_TAU
          TAU_STOP("hpx_process_delete");
#endif
}

