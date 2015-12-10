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

#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include "init.h"

hpx_action_handler_t hpx_action_get_handler(hpx_action_t id) {
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return (hpx_action_handler_t)entry->handler;
}

hpx_parcel_t *action_create_parcel(hpx_addr_t addr, hpx_action_t action,
                                   hpx_addr_t c_addr, hpx_action_t c_action,
                                   int n, ...) {
  CHECK_BOUND(actions, action);
  va_list args;
  va_start(args, n);
  const action_entry_t *entry = &actions[action];
  hpx_parcel_t *p =  entry->new_parcel(entry, addr, c_addr, c_action, n, &args);
  va_end(args);
  return p;
}

int action_execute(hpx_parcel_t *p) {
  dbg_assert(p->target != HPX_NULL);
  dbg_assert_str(p->action != HPX_ACTION_INVALID, "registration error\n");
  CHECK_BOUND(actions, p->action);

  hpx_action_t id = p->action;
  CHECK_BOUND(actions, id);
  const action_entry_t *entry = &actions[id];
  return entry->execute_parcel(entry, p);
}

int action_call_va(hpx_addr_t addr, hpx_action_t action, hpx_addr_t c_addr,
                   hpx_action_t c_action, hpx_addr_t lsync, hpx_addr_t gate,
                   int n, va_list *args) {
  CHECK_BOUND(actions, action);
  const action_entry_t *entry = &actions[action];
  hpx_parcel_t *p =  entry->new_parcel(entry, addr, c_addr, c_action, n, args);

  if (likely(!gate && !lsync)) {
    parcel_launch(p);
    return HPX_SUCCESS;
  }
  if (!gate && lsync) {
    return hpx_parcel_send(p, lsync);
  }
  if (!lsync) {
    return hpx_parcel_send_through_sync(p, gate);
  }
  return hpx_parcel_send_through(p, gate, lsync);
}
