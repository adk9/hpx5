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
/// @file locality.c
/// ----------------------------------------------------------------------------
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>                             // sysconf(...)
#include "hpx.h"
#include "locality.h"
#include "scheduler.h"
#include "sync/barriers.h"

typedef int (*_init_t)(void);
typedef void (*_fini_t)(void);

typedef struct _node {
  struct _node *next;
  _init_t init;
  _fini_t fini;
} _node_t;

static _node_t *_initializers = NULL;

void
locality_register_thread_callbacks(_init_t init, _fini_t fini) {
  _node_t *node = malloc(sizeof(*node));
  node->next = _initializers;
  node->init = init;
  node->fini = fini;
  _initializers = node;
}

static void
_free_thread_callbacks(void) {
  while (_initializers) {
    _node_t *node = _initializers;
    _initializers = node->next;
    free(node);
  }
}

static void
_do_fini(_node_t *node) {
  if (!node)
    return;

  if (node->fini)
    node->fini();

  _do_fini(node->next);
}

static int
_do_init(_node_t *node) {
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

/// ----------------------------------------------------------------------------
/// This locality's NULL ACTION.
/// ----------------------------------------------------------------------------
hpx_action_t HPX_ACTION_NULL = 0;

/// ----------------------------------------------------------------------------
/// Locality data.
/// ----------------------------------------------------------------------------
static int _n = 0;
static pthread_t *_threads = NULL;
static pthread_attr_t *_attributes = NULL;
static int *_args = NULL;
static sr_barrier_t *_barrier = NULL;

/// ----------------------------------------------------------------------------
/// Thread local data.
/// ----------------------------------------------------------------------------
static __thread int _id = 0;

/// ----------------------------------------------------------------------------
/// THE NULL ACTION
/// ----------------------------------------------------------------------------
static int _null_action(void *unused) {
  return HPX_SUCCESS;
}

static int _get_num_pu(void) {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

static void *_entry(void *args) {
  _id = *(int*)args;
  int e = _do_init(_initializers);
  if (!e)
    e = scheduler_startup(HPX_ACTION_NULL, NULL, 0);
  return (void*)(intptr_t)e;
}

int
locality_init_module(int n) {
  _n = (n) ? n : _get_num_pu();

  // register the null action for this locality
  HPX_ACTION_NULL = hpx_action_register("_null_action", _null_action);

  _threads = calloc(_n, sizeof(*_threads));
  if (!_threads) {
    perror("locality_init() Failed to allocate an array of pthread_t");
    goto unwind0;
  }

  _attributes = calloc(_n, sizeof(*_attributes));
  if (!_attributes) {
    perror("locality_init() failed to allocate an array of pthread_attr_t");
    goto unwind1;
  }

  _args = calloc(_n, sizeof(*_args));
  if (!_args) {
    perror("locality_init() failed to allocate an array of args (ints)");
    goto unwind2;
  }

  _barrier = sr_barrier_new(_n);
  if (!_barrier) {
    fprintf(stderr, "locality_init() failed to allocate its barrier.\n");
    goto unwind3;
  }

  // initialize the information for the main thread
  _threads[0] = pthread_self();
  pthread_getattr_np(_threads[0], &_attributes[0]);
  _args[0] = 0;

  // start the rest of the threads... they'll end up blocked at the barrier
  // until this thread joins the barrier in locality_start().
  int i;
  for (i = 1; i < _n; ++i) {
    _args[i] = i;
    errno = pthread_create(&_threads[i], &_attributes[i], _entry, &_args[i]);
    if (errno) {
      fprintf(stderr, "locality_init() failed to create thread #%d.\n", i);
      goto unwind4;
    }
  }

  for (int i = 0; i < _n; ++i) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(_id, &cpuset);
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
}

void
locality_fini_module(void) {
  for (int i = 1; i < _n; ++i)
    if (pthread_join(_threads[i], NULL))
      fprintf(stderr, "locality_init() could not clean up thread #%d.\n", i);
  sr_barrier_delete(_barrier);
  free(_args);
  free(_attributes);
  free(_threads);
  _free_thread_callbacks();
}

int
hpx_get_my_thread_id(void) {
  return _id;
}

int
hpx_get_num_threads(void) {
  return _n;
}
