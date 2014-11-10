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


/// Implement the locality actions.
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/scheduler.h"
#include "libhpx/system.h"

locality_t *here = NULL;

hpx_action_t locality_shutdown = 0;
hpx_action_t locality_call_continuation = 0;

/// The action that shuts down the HPX scheduler.
static int _shutdown_handler(void *UNUSED) {
  scheduler_shutdown(here->sched, LIBHPX_OK);
  return HPX_SUCCESS;
}

static int _call_cont_handler(locality_cont_args_t *args) {
  hpx_addr_t target = hpx_thread_current_target();
  if (!hpx_gas_try_pin(target, NULL))
    return HPX_RESEND;

  uint32_t size = hpx_thread_current_args_size() - sizeof(args->status) - sizeof(args->action);
  // handle status here: args->status;
  return hpx_call(target, args->action, args->data, size, HPX_NULL);
}

static HPX_CONSTRUCTOR void _init_actions(void) {
  locality_shutdown = HPX_REGISTER_ACTION(_shutdown_handler);
  locality_call_continuation = HPX_REGISTER_ACTION(_call_cont_handler);
}
