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

/// Implement the locality actions.
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>

locality_t *here = NULL;

/// The action that shuts down the HPX scheduler.
static int _locality_shutdown_handler(int src, uint64_t code) {
  dbg_assert(code < UINT64_MAX);
  log_net("received shutdown from %d (code %i)\n", src, (uint32_t)code);
  /*printf("received a shutdown from %d  --> to %d  : (code %i)\n", src, hpx_get_my_rank(), (uint32_t)code);*/
  scheduler_shutdown(here->sched, (uint32_t)code);
  return HPX_SUCCESS;
}
LIBHPX_ACTION(HPX_INTERRUPT, 0, locality_shutdown, _locality_shutdown_handler,
              HPX_INT, HPX_UINT64);


/// API to indicate shutting down of entire locality
/// this is different from shutdown handler which is to check/enforce exit per hpx_run
void _locality_shutdown(locality_t* loc){
  sync_store(&loc->reent_state.loc_shutdown, 0, SYNC_RELEASE);
}


/// API to check shutting down of entire locality
/// this is different from shutdown handler which is to check exit per hpx_run
int _locality_ready_for_shutdown(locality_t* loc){
  int shutdown = sync_load(&loc->reent_state.loc_shutdown, SYNC_ACQUIRE);
  return (shutdown != INT_MAX);
}
