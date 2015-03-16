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
# include <config.h>
#endif

#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/stats.h>
#include <libhpx/system.h>

#include "progress.h"

static void *_pthread_progress(void *arg) {
  int e = system_set_affinity_group(pthread_self(), system_get_cores()-1);
  if (e) {
    dbg_error("failed to bind heavyweight network thread.\n");
    return NULL;
  }

  locality_t *l = arg;
  global->join(global);
  if (e) {
    dbg_error("heavyweight network thread failed to join GAS\n");
    return NULL;
  }

  while (true) {
    pthread_testcancel();
    profile_ctr(scheduler_get_stats(l->sched)->progress++);
    int e = network_progress(l->network);
    if (e != LIBHPX_OK) {
      dbg_error("network progress error\n");
      return NULL;
    }
    pthread_yield();
  }
  return NULL;
}

pthread_t progress_start(locality_t *locality) {
  if (locality->config->network == HPX_NETWORK_SMP) {
    return pthread_self();
  }

  pthread_t id;
  int e = pthread_create(&id, NULL, _pthread_progress, locality);
  if (e) {
    dbg_error("failed to start network progress.\n");
    return pthread_self();
  }
  else {
    log("started network progress.\n");
  }
  return id;
}

void progress_stop(pthread_t thread) {
  if (pthread_equal(thread, pthread_self())) {
    return;
  }

  dbg_check(pthread_cancel(thread), "failed to cancel progress thread\n");
  dbg_check(pthread_join(thread, NULL), "failed to cancel progress thread\n");
}

