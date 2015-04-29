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
HPX_ACTION(HPX_INTERRUPT, 0, _locality_shutdown, locality_shutdown_handler,
           HPX_INT, HPX_UINT64);

int locality_call_continuation_handler(size_t n, locality_cont_args_t *args) {
  // just doing address translation, not pinning
  hpx_addr_t target = hpx_thread_current_target();
  if (!hpx_gas_try_pin(target, NULL)) {
    return HPX_RESEND;
  }

  uint32_t size = n - sizeof(args->status) - sizeof(args->action);
  // handle status here: args->status;
  return hpx_call(target, args->action, HPX_NULL, args->data, size);
}
HPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, locality_call_continuation,
           locality_call_continuation_handler, HPX_SIZE_T, HPX_POINTER);
