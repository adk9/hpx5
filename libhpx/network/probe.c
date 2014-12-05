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
# include "config.h"
#endif

#include <hpx/hpx.h>

#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>


static hpx_action_t _probe = 0;


static int _probe_handler(void *o) {
  network_t *network = *(network_t **)o;

  network_progress(network);

  hpx_parcel_t *stack = NULL;
  int e = hpx_call(HPX_HERE, _probe, &network, sizeof(network), HPX_NULL);
  if (e != HPX_SUCCESS)
    return e;

  while ((stack = network_probe(network, hpx_get_my_thread_id()))) {
    hpx_parcel_t *p = NULL;
    while ((p = parcel_stack_pop(&stack))) {
      scheduler_spawn(p);
    }
  }
  return HPX_SUCCESS;
}


static void HPX_CONSTRUCTOR _register_actions(void) {
  LIBHPX_REGISTER_ACTION(&_probe, _probe_handler);
}


int probe_start(network_t *network) {
  int e = hpx_call(HPX_HERE, _probe, &network, sizeof(network), HPX_NULL);
  if (e) {
    return dbg_error("failed to start network probe\n");
  }
  else {
    dbg_log("started probing the network.\n");
  }

  return LIBHPX_OK;
}


void probe_stop(void) {
}
