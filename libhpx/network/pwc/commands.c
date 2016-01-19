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
#include <libhpx/action.h>
#include <libhpx/gpa.h>
#include <libhpx/locality.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>

#include "commands.h"

static int _lco_set_handler(int src, uint64_t command) {
  hpx_addr_t lco = offset_to_gpa(here->rank, command);
  hpx_lco_set(lco, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}
COMMAND_DEF(lco_set, _lco_set_handler);

static int _lco_set_source_handler(int src, uint64_t command) {
  hpx_addr_t lco = offset_to_gpa(src, command);
  hpx_lco_set(lco, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}
COMMAND_DEF(lco_set_source, _lco_set_source_handler);

static int _delete_parcel_handler(int src, command_t command) {
  uintptr_t arg = command_get_arg(command);
  hpx_parcel_t *p = (hpx_parcel_t *)arg;
  log_net("releasing sent parcel %p\n", (void*)p);
  parcel_delete(p);
  return HPX_SUCCESS;
}
COMMAND_DEF(delete_parcel, _delete_parcel_handler);

/// Resume a parcel locally.
///
/// This command will resume a parcel that is suspended locally.
static int _resume_parcel_handler(int src, command_t command) {
  uintptr_t arg = command_get_arg(command);
  hpx_parcel_t *p = (hpx_parcel_t *)arg;
  log_net("resuming %s, (%p)\n", actions[p->action].key, (void*)p);
  parcel_launch(p);
  return HPX_SUCCESS;
}
COMMAND_DEF(resume_parcel, _resume_parcel_handler);

/// Resume a parcel at the source of an operation.
///
/// This remote cmd will bounce a resume operation back to the source of a
/// command during remote completion.
static int _resume_parcel_source_handler(int src, command_t command) {
  hpx_action_t cmd = resume_parcel;
  return network_command(here->network, HPX_THERE(src), cmd, command.packed);
}
COMMAND_DEF(resume_parcel_source, _resume_parcel_source_handler);

static HPX_USED const char *_straction(hpx_action_t id) {
  dbg_assert(here);
  CHECK_ACTION(id);
  return actions[id].key;
}

int command_run(int src, command_t cmd) {
  hpx_addr_t op = command_get_op(cmd);
  log_net("invoking command: %s from %d\n", _straction(op), src);
  CHECK_ACTION(op);
  command_handler_t f = (command_handler_t)(actions[op].handler);
  return f(src, cmd);
}
