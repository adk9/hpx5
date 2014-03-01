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
#define _GNU_SOURCE // pthread_getattr_np

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// @file libhpx/system.c
/// ----------------------------------------------------------------------------
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>                             // sysconf(...)
#include "hpx.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "manager.h"

/// Locality data.
static manager_t *_manager = NULL;

///
int
locality_init_module(const hpx_config_t *cfg) {
  int e = HPX_SUCCESS;

  // Initialize the manager.
  if (!(_manager = manager_new_mpirun()) ||
      !(_manager = manager_new_pmi()) ||
      !(_manager = manager_new_smp())) {
    e = 1;
    goto unwind0;
  }

  return e;

 unwind0:
  return e;
}

void
locality_fini_module(void) {
}

int
locality_get_rank(void) {
  return _manager->rank;
}

int
locality_get_n_ranks(void) {
  return _manager->n_ranks;
}

/// get the max number of processing units
int
locality_get_n_processors(void) {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

hpx_action_t
locality_action_register(const char *id, hpx_action_handler_t f) {
  return (hpx_action_t)f;
}

hpx_action_handler_t
locality_action_lookup(hpx_action_t key) {
  return (hpx_action_handler_t)key;
}


int
hpx_get_my_rank(void) {
  return locality_get_rank();
}


int
hpx_get_num_ranks(void) {
  return locality_get_n_ranks();
}


#if 0

/// A simple node structure to store an initializer and a finalizer.
typedef struct _node {
  struct _node *next;
  init_t init;
  fini_t fini;
} _node_t;

/// The stack of registered initializers.
static _node_t *_initializers = NULL;

/// Register an initializer/finalizer callback.
void
locality_thread_register_callbacks(init_t init, fini_t fini) {
  _node_t *node = malloc(sizeof(*node));
  node->next = _initializers;
  node->init = init;
  node->fini = fini;
  _initializers = node;
}

/// Free the list of registered callbacks.
static void _free_registered_callbacks(void) {
  while (_initializers) {
    _node_t *node = _initializers;
    _initializers = node->next;
    free(node);
  }
}

/// Run the list of finalizers in a pre-order list traversal.
///
/// We take the list argument as a void* to make this the right type for the
/// pthread_cleanup_push() macro.
static void _do_fini(void *args) {
  _node_t *node = args;
  if (!node)
    return;

  if (node->fini)
    node->fini();

  _do_fini(node->next);
}

/// Run the list of initializers in a post-order list traversal.
///
/// The initializers are stored as a stack, but we'd like to run them in the
/// order that they were registered, so we traverse the list recursively and run
/// the initializers bottom-up.
///
/// If we run into a problem with an initializer, we run all of the finalizers
/// for initializers that have already been run (in reverse order), and then
/// return the error.
static int _do_init(_node_t *node) {
  int e = 0;

  if (!node)
    return e;

  // do child initialization first
  if ((e = _do_init(node->next)))
    return e;

  if (!node->init)
    return e;

  // if my initialization fails, finalize my children
  if ((e = node->init()))
    _do_fini(node->next);

  return e;
}

  int processors = _get_num_pu();
  _n_threads = (n) ? n : processors;

  // register the null action for this locality
  HPX_ACTION_NULL = hpx_action_register("_null_action", _null_action);

  _threads = calloc(_n_threads, sizeof(*_threads));
  if (!_threads) {
    perror("locality_init() Failed to allocate an array of pthread_t");
    goto unwind0;
  }

  _attributes = calloc(_n_threads, sizeof(*_attributes));
  if (!_attributes) {
    perror("locality_init() failed to allocate an array of pthread_attr_t");
    goto unwind1;
  }

  _args = calloc(_n_threads, sizeof(*_args));
  if (!_args) {
    perror("locality_init() failed to allocate an array of args (ints)");
    goto unwind2;
  }

  // initialize the information for the main thread
  _threads[0] = pthread_self();
  pthread_getattr_np(_threads[0], &_attributes[0]);
  _args[0] = 0;

  // start the rest of the threads... they'll end up blocked at the barrier
  // until this thread joins the barrier in locality_start().
  int i;
  for (i = 1; i < _n_threads; ++i) {
    _args[i] = i;
    errno = pthread_create(&_threads[i], &_attributes[i], _native_thread_entry,
                           &_args[i]);
    if (errno) {
      fprintf(stderr, "locality_init() failed to create thread #%d.\n", i);
      goto unwind4;
    }
  }

  // set affinities so that the native threads don't move around
  for (int i = 0; i < _n_threads; ++i) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(_id % processors, &cpuset);
    pthread_setaffinity_np(_threads[_id], sizeof(cpuset), &cpuset);
  }

  return 0;

 unwind4:
  for (int j = 1; j < i; ++j)
    if (pthread_cancel(_threads[j]) || pthread_join(_threads[j], NULL))
      fprintf(stderr, "locality_init() could not clean up thread #%d.\n", j);
  sr_barrier_delete(_barrier);
 unwind3:
  free(_args);
 unwind2:
  free(_attributes);
 unwind1:
  free(_threads);
 unwind0:
  return errno;

  for (int i = 1; i < _n_threads; ++i)
    if (pthread_join(_threads[i], NULL))
      fprintf(stderr, "locality_init() could not clean up thread #%d.\n", i);
  free(_args);
  free(_attributes);
  free(_threads);
  _free_registered_callbacks();

#endif
