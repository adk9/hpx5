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
#include <signal.h>
#include <setjmp.h>
#include "locality.h"
#include "scheduler.h"
#include "network.h"
#include "thread.h"
#include "entry.h"
#include "debug.h"


static int _n_threads = 0;
static pthread_t *_threads = NULL;
static __thread int _thread = 0;
static __thread sigjmp_buf _native_stack;

/// ----------------------------------------------------------------------------
/// A thread_transfer() continuation that runs after the first transfer.
/// ----------------------------------------------------------------------------
static int _on_entry(void *sp, void *env) {
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// A signal handler that deals with the shutdown signal.
/// ----------------------------------------------------------------------------
static void _shutdown(int sig, siginfo_t *info, void *ctx) {
  assert(sig == SIGUSR1);
  siglongjmp(_native_stack, 1);
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
  // the native stack is used during shutdown
  if (sigsetjmp(_native_stack, 1)) {
    scheduler_thread_shutdown();
    pthread_exit(0);
  }

  _thread = (int)(intptr_t)arg;

  hpx_parcel_t *p = hpx_parcel_acquire(0);
  if (!p) {
    locality_printe("failed to allocate a parcel in scheduler thread entry"
                    " %d.\n", _thread);
    return (void*)(intptr_t)errno;
  }

  thread_t *t = thread_new(scheduler_thread_entry, p);
  if (!t) {
    locality_printe("failed to allocate a thread in scheduler thread entry"
                    " %d.\n", _thread);
    network_release(p);
    return (void*)(intptr_t)errno;
  }

  // we need asynchronous cancellation to shutdown the HPX scheduler correctly,
  // this should be reevaluated later
  int e = thread_transfer(t->sp, NULL, _on_entry);
  return (void*)(intptr_t)e;
}


/// Starts the scheduler.
int
scheduler_startup(const hpx_config_t *cfg) {
  int e = HPX_SUCCESS;

  // set the stack size
  thread_set_stack_size(cfg->stack_bytes);

  // figure out how many scheduler threads we want to spawn
  _n_threads = cfg->scheduler_threads;
  if (!_n_threads)
    _n_threads = locality_get_n_processors();

  // allocate the array of pthread descriptors
  _threads = calloc(_n_threads, sizeof(_threads[0]));
  if (!_threads) {
    locality_printe("failed to allocate thread table.\n");
    e = errno;
    goto unwind0;
  }

  // register the shutdown signal
  struct sigaction sa;
  sa.sa_sigaction = _shutdown;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  e = sigaction(SIGUSR1, &sa, NULL);
  if (e) {
    locality_printe("failed to register the shutdown handler.\n");
    goto unwind1;
  }

  // start all of the scheduler threads
  int i;

  for (i = 0; i < _n_threads; ++i) {
    void *arg = (void*)(intptr_t)i;
    int e = pthread_create(&_threads[i], NULL, _thread_entry, arg);
    if (e) {
      locality_printe("failed to create scheduler thread #%d.\n", i);
      goto unwind2;
    }
  }

  // set all of the affinities
  for (i = 0; i < _n_threads; ++i) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(i % locality_get_n_processors(), &cpuset);
    e = pthread_setaffinity_np(_threads[i], sizeof(cpuset), &cpuset);
    if (e) {
      locality_printe("failed to bind thread affinity for %d", i);
      goto unwind3;
    }
  }

  return HPX_SUCCESS;

 unwind3:
  i = _n_threads;

 unwind2:
  for (int j = 0; j < i; ++j)
    if (pthread_kill(_threads[j], SIGUSR1) || pthread_join(_threads[j], NULL))
      locality_printe("could not clean up thread #%d.\n", j);

 unwind1:
  free(_threads);

 unwind0:
  return e;
}


/// Finalizes the scheduler.
///
/// @todo: Right now this just pulls the rug out of all of the running
///        threads. We should come up with a better way to do this that gives
///        threads a chance to clean up.
void
scheduler_shutdown(void) {
  for (int i = 0; i < _n_threads; ++i)
    if (pthread_kill(_threads[i], SIGUSR1) || pthread_join(_threads[i], NULL))
      locality_printe("could not clean up thread #%d.\n", i);
}


int
hpx_get_my_thread_id(void) {
  return _thread;
}
