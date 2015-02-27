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
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/scheduler.h"
#include "libhpx/system.h"

locality_t *here = NULL;

/// The action that shuts down the HPX scheduler.
static int _locality_shutdown_handler(int src, uint64_t code) {
  dbg_assert(code < UINT64_MAX);
  log_net("received shutdown from %d (code %i)\n", src, (uint32_t)code);
  scheduler_shutdown(here->sched, (uint32_t)code);
  return HPX_SUCCESS;
}
HPX_ACTION_DEF(INTERRUPT, _locality_shutdown_handler, locality_shutdown,
               HPX_INT, HPX_UINT64);

HPX_ACTION(locality_call_continuation, locality_cont_args_t *args) {
  // just doing address translation, not pinning
  hpx_addr_t target = hpx_thread_current_target();
  if (!hpx_gas_try_pin(target, NULL)) {
    return HPX_RESEND;
  }

  uint32_t size = hpx_thread_current_args_size() - sizeof(args->status) - sizeof(args->action);
  // handle status here: args->status;
  return hpx_call(target, args->action, HPX_NULL, args->data, size);
}
