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

#include <hpx/hpx.h>

#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/instrumentation.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>


#ifdef HAVE_PHOTON
#include <photon.h>
#include "../process/allreduce.h"
#endif

static int _probe_handler(network_t *network) {
  hpx_parcel_t *stack = NULL,
                   *p = NULL;

  while (!scheduler_is_shutdown(here->sched)) {
    INST(uint64_t start_time = hpx_time_to_ns(hpx_time_now()));

    while ((stack = network_probe(network, hpx_get_my_thread_id()))) {
      while ((p = parcel_stack_pop(&stack))) {
        EVENT_PARCEL_RECV(p);
        parcel_launch(p);
      }
    }

#ifdef HAVE_PHOTON
    photon_rid hwrreq;
    void *ctx = NULL;
    double value;
    int rc = photon_hw_collective_probe((void*)&value, &ctx, &hwrreq);
    if (ctx) {
      allreduce_t *r = (allreduce_t*)ctx;
      // just trigger the continuation stored in this node
      allreduce_bcast(r, (const void*)&value);
    }
#endif
    
    inst_trace(HPX_INST_SCHEDTIMES, HPX_INST_SCHEDTIMES_PROBE, start_time);
    hpx_thread_yield();
  }

  return HPX_SUCCESS;
}

static int _progress_handler(network_t *network) {
  while (!scheduler_is_shutdown(here->sched)) {
    INST(uint64_t start_time = hpx_time_to_ns(hpx_time_now()));
    network_progress(network);
    inst_trace(HPX_INST_SCHEDTIMES, HPX_INST_SCHEDTIMES_PROGRESS, start_time);
    hpx_thread_yield();
  }

  return HPX_SUCCESS;
}

static LIBHPX_ACTION(HPX_DEFAULT, 0, _probe, _probe_handler, HPX_POINTER);
static LIBHPX_ACTION(HPX_DEFAULT, 0, _progress, _progress_handler, HPX_POINTER);

int probe_start(network_t *network) {
  // NB: we should encapsulate this probe infrastructure inside of the networks
  // themselves, but for now we just avoid probing for SMP.
  if (network->type == HPX_NETWORK_SMP) {
    return HPX_SUCCESS;
  }

  hpx_parcel_t *p = NULL;
  p = action_create_parcel(HPX_HERE, _probe, HPX_NULL, 0, 1, &network);
  dbg_assert_str(p, "failed to acquire network probe parcel\n");
  scheduler_spawn(p);
  log_net("started probing the network\n");

  p = action_create_parcel(HPX_HERE, _progress, HPX_NULL, 0, 1, &network);
  dbg_assert_str(p, "failed to acquire network progress parcel\n");
  scheduler_spawn(p);
  log_net("starting progressing the network\n");

  return LIBHPX_OK;
}

void probe_stop(void) {
}
