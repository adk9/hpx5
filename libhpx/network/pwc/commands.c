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
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include "commands.h"

static int _release_parcel_handler(int src, command_t command) {
  uintptr_t arg = command_get_arg(command);
  hpx_parcel_t *p = (hpx_parcel_t *)arg;
  log_net("releasing sent parcel %p\n", (void*)p);
  hpx_parcel_release(p);
  return HPX_SUCCESS;
}
COMMAND_DEF(INTERRUPT, _release_parcel_handler, release_parcel);

static int _recv_parcel_handler(int src, command_t command) {
  const void *addr = (const void*)command_get_arg(command);
  const hpx_parcel_t *p = addr;
  // todo: don't make this copy
  hpx_parcel_t *clone = hpx_parcel_acquire(NULL, parcel_payload_size(p));
  memcpy(clone, p, parcel_size(p));
  clone->src = src;
  scheduler_spawn(clone);
  return HPX_SUCCESS;
}
COMMAND_DEF(INTERRUPT, _recv_parcel_handler, recv_parcel);


static int _rendezvous_launch_handler(int src, command_t cmd) {
  uintptr_t arg = command_get_arg(cmd);
  hpx_parcel_t *p = (void*)arg;
  scheduler_spawn(p);
  return HPX_SUCCESS;
}
COMMAND_DEF(INTERRUPT, _rendezvous_launch_handler, rendezvous_launch);
