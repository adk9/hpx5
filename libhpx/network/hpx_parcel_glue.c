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

#include <string.h>
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/attach.h>
#include <libhpx/debug.h>
#include <libhpx/scheduler.h>
#include <libhpx/parcel.h>

static const size_t HPX_SMALL_THRESHOLD = HPX_PAGE_SIZE;

hpx_parcel_t *hpx_parcel_acquire(const void *buffer, size_t bytes) {
  hpx_addr_t target = HPX_HERE;
  hpx_pid_t pid = hpx_thread_current_pid();
  return parcel_new(target, 0, 0, 0, pid, buffer, bytes);
}

hpx_status_t hpx_parcel_send_sync(hpx_parcel_t *p) {
  INST_EVENT_PARCEL_SEND(p);
  return parcel_launch(p);
}

static HPX_ACTION_DEF(DEFAULT, hpx_parcel_send_sync, _send_async, HPX_POINTER);

hpx_status_t hpx_parcel_send(hpx_parcel_t *p, hpx_addr_t lsync) {
  if (p->size < HPX_SMALL_THRESHOLD || p->state.serialized) {
    hpx_status_t status = parcel_launch(p);
    hpx_lco_error(lsync, status, HPX_NULL);
    return status;
  }
  else {
    return hpx_call(HPX_HERE, _send_async, lsync, &p);
  }
}

hpx_status_t hpx_parcel_send_through_sync(hpx_parcel_t *p, hpx_addr_t gate) {
  dbg_assert(p->target != HPX_NULL);
  int e = hpx_call(gate, attach, HPX_NULL, p, parcel_size(p));
  parcel_delete(p);
  return e;
}

static int _delete_send_through_parcel_handler(hpx_parcel_t *p) {
  hpx_addr_t lsync = hpx_thread_current_target();
  hpx_lco_wait(lsync);
  parcel_delete(p);
  return HPX_SUCCESS;
}
static HPX_ACTION_DEF(DEFAULT, _delete_send_through_parcel_handler,
                      _delete_send_through_parcel, HPX_POINTER);

hpx_status_t hpx_parcel_send_through(hpx_parcel_t *p, hpx_addr_t gate,
                                     hpx_addr_t lsync) {
  dbg_assert(p->target != HPX_NULL);
  hpx_parcel_t *pattach = parcel_new(gate, attach, 0, 0,
                                     hpx_thread_current_pid(), p,
                                     parcel_size(p));

  int e = hpx_parcel_send(pattach, lsync);
  if (lsync) {
    int e = hpx_call(lsync, _delete_send_through_parcel, HPX_NULL, &p);
    dbg_check(e, "failed call\n");
  }
  else {
    parcel_delete(p);
  }
  return e;
}

void hpx_parcel_release(hpx_parcel_t *p) {
  parcel_delete(p);
}

void hpx_parcel_set_action(hpx_parcel_t *p, hpx_action_t action) {
  p->action = action;
}

void hpx_parcel_set_target(hpx_parcel_t *p, hpx_addr_t addr) {
  p->target = addr;
}

void hpx_parcel_set_cont_action(hpx_parcel_t *p, hpx_action_t action) {
  p->c_action = action;
}

void hpx_parcel_set_cont_target(hpx_parcel_t *p, hpx_addr_t cont) {
  p->c_target = cont;
}

void hpx_parcel_set_data(hpx_parcel_t *p, const void *data, int size) {
  if (size) {
    void *to = hpx_parcel_get_data(p);
    memmove(to, data, size);
  }
}

void _hpx_parcel_set_args(hpx_parcel_t *p, int n, ...) {
  va_list vargs;
  va_start(vargs, n);
  action_pack_args(p, n, &vargs);
  va_end(vargs);
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
  if (p->size == 0) {
    return NULL;
  }

  if (p->state.serialized) {
    return (void*)&p->buffer;
  }

  // Copy out the pointer stored in the bufferm but don't violate strict
  // aliasing.
  void *buffer = NULL;
  memcpy(&buffer, &p->buffer, sizeof(buffer));
  return buffer;
}

hpx_pid_t hpx_parcel_get_pid(const hpx_parcel_t *p) {
  return p->pid;
}
