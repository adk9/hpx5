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

#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"


static HPX_ACTION(_probe, void *o) {
  network_t *network = *(network_t **)o;

  while (true) {
    network_progress(network);
    hpx_parcel_t *stack = NULL;
    while ((stack = network_probe(network, hpx_get_my_thread_id()))) {
      hpx_parcel_t *p = NULL;
      while ((p = parcel_stack_pop(&stack))) {
        parcel_launch(p);
      }
    }
    hpx_thread_yield();
  }

  return HPX_SUCCESS;
}

int probe_start(network_t *network) {
  // NB: we should encapsulate this probe infrastructure inside of the networks
  // themselves, but for now we just avoid probing for SMP.
  if (network->type == LIBHPX_NETWORK_SMP) {
    return HPX_SUCCESS;
  }

  int e = hpx_call(HPX_HERE, _probe, HPX_NULL, &network, sizeof(network));
  dbg_check(e, "failed to start network probe\n");
  log_net("started probing the network\n");
  return LIBHPX_OK;
}

void probe_stop(void) {
}
