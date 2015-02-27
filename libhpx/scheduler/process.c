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

/// @file libhpx/scheduler/process.c
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "libsync/sync.h"
#include "libhpx/debug.h"
#include "libhpx/action.h"
#include "libhpx/locality.h"
#include "libhpx/parcel.h"
#include "libhpx/process.h"
#include "libhpx/scheduler.h"
#include "termination.h"


typedef struct {
  volatile uint64_t    credit;               // credit balance
  bitmap_t              *debt;               // the credit that was recovered
  hpx_addr_t      termination;               // the termination LCO
} _process_t;


/// Remote action interface to a process.
static HPX_ACTION_DECL(_proc_call);
static HPX_ACTION_DECL(_proc_delete);
static HPX_ACTION_DECL(_proc_return_credit);

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
  hpx_addr_t   target;
  hpx_action_t action;
  hpx_addr_t   result;
  char         data[];
} _call_args_t;

static HPX_ACTION(_proc_call, _call_args_t *args) {
  hpx_addr_t process = hpx_thread_current_target();
  _process_t *p = NULL;
  if (!hpx_gas_try_pin(process, (void**)&p)) {
    return HPX_RESEND;
  }

  uint64_t credit = sync_addf(&p->credit, 1, SYNC_ACQ_REL);
  hpx_gas_unpin(process);

  hpx_pid_t pid = hpx_process_getpid(process);
  uint32_t len = hpx_thread_current_args_size() - sizeof(*args);
  hpx_parcel_t *parcel = parcel_create(args->target, args->action, args->data,
                                       len, args->result,
                                       hpx_lco_set_action, pid, true);
  if (!parcel) {
    dbg_error("process: call_action failed.\n");
  }
  parcel_set_credit(parcel, credit);

  hpx_parcel_send_sync(parcel);
  return HPX_SUCCESS;
}


static HPX_PINNED(_proc_delete, void *args) {
  _process_t *p = hpx_thread_current_local_target();
  assert(p);
  _free(p);
  return HPX_SUCCESS;
}


static HPX_PINNED(_proc_return_credit, uint64_t *args) {
  _process_t *p = hpx_thread_current_local_target();
  assert(p);

  // add credit to the credit-accounting bitmap
  if (cr_bitmap_add_and_test(p->debt, *args)) {
    //log("detected quiescence...\n");
    uint64_t total_credit = sync_addf(&p->credit, -1, SYNC_ACQ_REL);
    assert(total_credit == 0);
    assert(_is_tracked(p));
    hpx_lco_set(p->termination, 0, NULL, HPX_NULL, HPX_NULL);
  }
  return HPX_SUCCESS;
}


int process_recover_credit(hpx_parcel_t *p) {
  hpx_addr_t process = p->pid;
  if (process == HPX_NULL) {
    return HPX_SUCCESS;
  }

  if (!p->credit) {
    return HPX_SUCCESS;
  }

  hpx_parcel_t *pp = parcel_create(process, _proc_return_credit, &p->credit,
                                   sizeof(p->credit), HPX_NULL, HPX_ACTION_NULL,
                                   0, false);
  if (!pp) {
    dbg_error("parcel_recover_credit failed.\n");
  }
  parcel_set_credit(pp, 0);

  hpx_parcel_send_sync(pp);
  return HPX_SUCCESS;
}


hpx_addr_t hpx_process_new(hpx_addr_t termination) {
  if (termination == HPX_NULL)
    return HPX_NULL;
  _process_t *p;
  hpx_addr_t process = hpx_gas_alloc(sizeof(*p));
  if (!hpx_gas_try_pin(process, (void**)&p)) {
    dbg_error("Could not pin newly allocated process.\n");
  }
  _init(p, termination);
  hpx_gas_unpin(process);
  return process;
}


hpx_pid_t hpx_process_getpid(hpx_addr_t process) {
  return (hpx_pid_t)process;
}


int hpx_process_call(hpx_addr_t process, hpx_addr_t addr, hpx_action_t action,
                     const void *args, size_t len, hpx_addr_t result)
{
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, len + sizeof(_call_args_t));
  hpx_parcel_set_target(p, process);
  hpx_parcel_set_action(p, _proc_call);
  hpx_parcel_set_pid(p, 0);
  parcel_set_credit(p, 0);

  _call_args_t *call_args = (_call_args_t *)hpx_parcel_get_data(p);
  call_args->result = result;
  call_args->target = addr;
  call_args->action = action;
  memcpy(&call_args->data, args, len);

  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}


/// Deletes a process.
void hpx_process_delete(hpx_addr_t process, hpx_addr_t sync) {
  if (process == HPX_NULL)
    return;

  hpx_call_sync(process, _proc_delete, NULL, 0, NULL, 0);
  hpx_gas_free(process, sync);
}

