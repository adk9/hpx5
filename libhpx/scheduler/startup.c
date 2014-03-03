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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "locality.h"
#include "scheduler.h"
#include "network.h"
#include "thread.h"
#include "entry.h"
#include "debug.h"

static int _thread = 0;
static int _n_threads = 0;
static pthread_t *_threads = NULL;

/// ----------------------------------------------------------------------------
/// A thread_transfer() continuation that runs after the first transfer.
///
/// @todo: We could do something intelligent with this, like checkpointing the
///        native stack so that we could transfer back to it on shutdown.
/// ----------------------------------------------------------------------------
static int _on_entry(void *sp, void *env) {
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Entry point for a native scheduler thread.
///
/// Native threads jump into the scheduler by starting a NULL action. This
/// action essentially implements the scheduler logic in a loop, until something
/// appears in the new parcel queue to process. This something might either come
/// from the network initialization, from the network itself, or from the main
/// thread.
///
/// @param arg - the thread's ID, cast to a void*
/// @returns   - the result of the initial thread_transfer
/// ----------------------------------------------------------------------------
static void *_thread_entry(void *arg) {
  _thread = (int)(intptr_t)arg;

  hpx_parcel_t *p = hpx_parcel_acquire(0);
  if (!p) {
    printe("failed to allocate a parcel in scheduler thread entry %d.\n", _thread);
    return (void*)(intptr_t)errno;
  }
  thread_t *t = thread_new(scheduler_thread_entry, p);
  if (!t) {
    printe("failed to allocate a thread in scheduler thread entry %d.\n", _thread);
    network_release(p);
    return (void*)(intptr_t)errno;
  }

  return (void*)(intptr_t) thread_transfer(t->sp, NULL, _on_entry);
}


/// Starts the scheduler.
int
scheduler_startup(const hpx_config_t *cfg) {
  // set the stack size
  thread_set_stack_size(cfg->stack_bytes);

  // figure out how many scheduler threads we want to spawn
  _n_threads = cfg->scheduler_threads;
  if (!_n_threads)
    _n_threads = locality_get_n_processors();

  // allocate the array of pthread descriptors
  _threads = calloc(_n_threads, sizeof(_threads[0]));
  if (!_threads) {
    printe("failed to allocate thread table.\n");
    return errno;
  }

  // start all of the scheduler threads
  for (int i = 0; i < _n_threads; ++i) {
    void *arg = (void*)(intptr_t)i;
    int e = pthread_create(&_threads[i], NULL, _thread_entry, arg);
    if (!e)
      continue;

    // error branch
    printe("failed to create scheduler thread #%d.\n", i);
    for (int j = 0; j < i; ++j)
      if (pthread_cancel(_threads[j]) || pthread_join(_threads[j], NULL))
        printe("could not clean up thread #%d.\n", j);

    return e;
  }

  return HPX_SUCCESS;
}


/// Finalizes the scheduler.
///
/// @todo: Right now this just pulls the rug out of all of the running
///        threads. We should come up with a better way to do this that gives
///        threads a chance to clean up.
void
scheduler_shutdown(void) {
  for (int i = 0; i < _n_threads; ++i)
    if (pthread_cancel(_threads[i]) || pthread_join(_threads[i], NULL))
      printe("could not clean up thread #%d.\n", i);
}


int
hpx_get_my_thread_id(void) {
  return _thread;
}
