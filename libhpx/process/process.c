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

#include <libsync/sync.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include <libhpx/process.h>
#include <libhpx/scheduler.h>
#include <libhpx/termination.h>

typedef struct {
  volatile uint64_t    credit;               // credit balance
  bitmap_t              *debt;               // the credit that was recovered
  hpx_addr_t      termination;               // the termination LCO
} _process_t;


/// Remote action interface to a process.
static HPX_ACTION_DECL(_proc_call);
static HPX_ACTION_DECL(_proc_delete);
static HPX_ACTION_DECL(_proc_return_credit);

static bool HPX_USED _is_tracked(_process_t *p) {
  return (p->termination != HPX_NULL);
}

/// Remote action to delete a process.
static void _free(_process_t *p) {
  if (!p) {
    return;
  }

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

static int _proc_call_handler(hpx_parcel_t *arg, size_t n) {
  hpx_addr_t process = hpx_thread_current_target();
  _process_t *p = NULL;
  if (!hpx_gas_try_pin(process, (void**)&p)) {
    return HPX_RESEND;
  }

  uint64_t credit = sync_addf(&p->credit, 1, SYNC_ACQ_REL);
  hpx_gas_unpin(process);

  hpx_pid_t pid = hpx_process_getpid(process);
  hpx_parcel_t *parcel = hpx_parcel_acquire(NULL, parcel_size(arg));
  memcpy(parcel, arg, parcel_size(arg));
  hpx_parcel_set_pid(parcel, pid);
  parcel->credit = credit;
  hpx_parcel_send_sync(parcel);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _proc_call,
                     _proc_call_handler, HPX_POINTER, HPX_SIZE_T);

static int _proc_delete_handler(_process_t *p, size_t size) {
  _free(p);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED | HPX_MARSHALLED, _proc_delete,
                     _proc_delete_handler,
                     HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

static int _proc_return_credit_handler(_process_t *p, uint64_t *args, size_t size) {
  // add credit to the credit-accounting bitmap
  uint64_t debt = cr_bitmap_add_and_test(p->debt, *args);
  for (;;) {
    uint64_t credit = sync_load(&p->credit, SYNC_ACQUIRE);
    if ((credit != 0) && ~(debt | ((UINT64_C(1) << (64-credit)) - 1)) == 0) {
      // log("detected quiescence...\n");
      if (!sync_cas(&p->credit, credit, -credit, SYNC_RELEASE, SYNC_RELAXED)) {
        continue;
      }
      dbg_assert(_is_tracked(p));
      hpx_lco_set(p->termination, 0, NULL, HPX_NULL, HPX_NULL);
    }
    break;
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED | HPX_MARSHALLED, _proc_return_credit,
                     _proc_return_credit_handler,
                     HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

int process_recover_credit(hpx_parcel_t *p) {
  hpx_addr_t process = p->pid;
  if (process == HPX_NULL) {
    return HPX_SUCCESS;
  }

  if (!p->credit) {
    return HPX_SUCCESS;
  }

  hpx_parcel_t *pp = parcel_new(process, _proc_return_credit, 0, 0, 0,
                                &p->credit, sizeof(p->credit));
  if (!pp) {
    dbg_error("parcel_recover_credit failed.\n");
  }
  pp->credit = 0;

  hpx_parcel_send_sync(pp);
  return HPX_SUCCESS;
}

hpx_addr_t hpx_process_new(hpx_addr_t termination) {
  if (termination == HPX_NULL) {
    return HPX_NULL;
  }

  _process_t *p;
  hpx_addr_t process = hpx_gas_alloc_local(1, sizeof(*p), 0);
  if (!hpx_gas_try_pin(process, (void**)&p)) {
    dbg_error("Could not pin newly allocated process.\n");
  }
  _init(p, termination);
  hpx_gas_unpin(process);

#ifdef ENABLE_INSTRUMENTATION
  inst_trace(HPX_INST_CLASS_PROCESS, HPX_INST_EVENT_PROCESS_NEW,
             process, termination);
#endif
  return process;
}

hpx_pid_t hpx_process_getpid(hpx_addr_t process) {
  return (hpx_pid_t)process;
}

int _hpx_process_call(hpx_addr_t process, hpx_addr_t addr, hpx_action_t action,
                      hpx_addr_t result, int n, ...) {
  va_list vargs;
  va_start(vargs, n);
  hpx_parcel_t *parcel = action_create_parcel_va(addr, action, result,
                                                 hpx_lco_set_action, n, &vargs);
  va_end(vargs);

  if (hpx_thread_current_pid() == hpx_process_getpid(process)) {
    hpx_parcel_send_sync(parcel);
    return HPX_SUCCESS;
  }

  hpx_addr_t sync = hpx_lco_future_new(0);
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, parcel_size(parcel));
  p->target = process;
  p->action = _proc_call;
  p->c_target = sync;
  p->c_action = hpx_lco_set_action;
  hpx_parcel_set_data(p, parcel, parcel_size(parcel));
  p->pid = 0;
  p->credit = 0;
#ifdef ENABLE_INSTRUMENTATION
  inst_trace(HPX_INST_CLASS_PROCESS, HPX_INST_EVENT_PROCESS_CALL,
             process, p->pid);
#endif
  hpx_parcel_send_sync(p);

  parcel_delete(parcel);
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);
  return HPX_SUCCESS;
}

/// Delete a process.
void hpx_process_delete(hpx_addr_t process, hpx_addr_t sync) {
  if (process == HPX_NULL) {
    return;
  }

  hpx_call_sync(process, _proc_delete, NULL, 0, NULL, 0);
  hpx_gas_free(process, sync);
#ifdef ENABLE_INSTRUMENTATION
  inst_trace(HPX_INST_CLASS_PROCESS, HPX_INST_EVENT_PROCESS_DELETE, process);
#endif
}

