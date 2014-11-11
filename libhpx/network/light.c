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
#include "config.h"
#endif

#include <pthread.h>
#include "libhpx/action.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "libhpx/stats.h"
#include "libhpx/transport.h"
#include "servers.h"

hpx_action_t light_network = 0;

static int _light_network_action(void *args) {
  while (true) {
    profile_ctr(thread_get_stats()->progress++);
    transport_progress(here->transport, TRANSPORT_POLL);
    scheduler_yield();
  }
  return HPX_SUCCESS;
}


static void HPX_CONSTRUCTOR _init_actions(void) {
  LIBHPX_REGISTER_ACTION(&light_network, _light_network_action);
}
