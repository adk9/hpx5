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
#include "config.h"
#endif

/// @file libhpx/par.c
/// @brief Implements the HPX par constructs
///

#include <stdlib.h>
#include <string.h>
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/scheduler.h>


static int _par_for_async_handler(hpx_for_action_t f, void *args, int min, int max) {
  for (int i = min, e = max; i < e; ++i) {
    f(i, args);
  }
  return HPX_SUCCESS;
}

static LIBHPX_ACTION(HPX_DEFAULT, 0, _par_for_async, _par_for_async_handler,
                     HPX_POINTER, HPX_POINTER, HPX_INT, HPX_INT);

int hpx_par_for(hpx_for_action_t f, const int min, const int max,
                const void *args, hpx_addr_t sync) {
  dbg_assert(max - min > 0);

  // get the number of scheduler threads
  int nthreads = HPX_THREADS;

  hpx_addr_t and = HPX_NULL;
  if (sync) {
    and = hpx_lco_and_new(nthreads);
    hpx_call_when_with_continuation(and, sync, hpx_lco_set_action,
                                    and, hpx_lco_delete_action, NULL, 0);
  }

  const int n = max - min;
  const int m = n / nthreads;
  int r = n % nthreads;
  int rmin = min;
  int rmax = max;

  int base = rmin;
  for (int i = 0, e = nthreads; i < e; ++i) {
    rmin = base;
    rmax = base + m + ((r-- > 0) ? 1 : 0);
    base = rmax;
    int e = hpx_call(HPX_HERE, _par_for_async, and, &f, &args, &rmin, &rmax);
    if (e) {
      return e;
    }
  }

  return HPX_SUCCESS;
}

int hpx_par_for_sync(hpx_for_action_t f, const int min, const int max,
                     const void *args) {
  dbg_assert(max - min > 0);
  hpx_addr_t sync = hpx_lco_future_new(0);
  if (sync == HPX_NULL) {
    return log_error("could not allocate an LCO.\n");
  }

  int e = hpx_par_for(f, min, max, args, sync);
  if (!e) {
    e = hpx_lco_wait(sync);
  }
  hpx_lco_delete(sync, HPX_NULL);
  return e;
}


/// HPX parallel "call".

typedef struct {
  hpx_action_t action;
  int min;
  int max;
  int branching_factor;
  int cutoff;
  size_t arg_size;
  void (*arg_init)(void*, const int, const void*);
  hpx_addr_t sync;
  char env[];
} par_call_async_args_t;

static int
_hpx_par_call_helper(hpx_action_t action, const int min,
                     const int max, const int branching_factor,
                     const int cutoff,
                     const size_t arg_size,
                     void (*arg_init)(void*, const int, const void*),
                     const size_t env_size, const void *env,
                     hpx_addr_t sync);

static int _par_call_async_handler(par_call_async_args_t *args, size_t n) {
  const size_t env_size = n - sizeof(*args);
  return _hpx_par_call_helper(args->action, args->min, args->max, args->branching_factor,
                              args->cutoff, args->arg_size, args->arg_init, env_size,
                              &args->env, args->sync);
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _par_call_async,
                     _par_call_async_handler, HPX_POINTER, HPX_SIZE_T);

static int
_hpx_par_call_helper(hpx_action_t action, const int min,
                     const int max, const int branching_factor,
                     const int cutoff,
                     const size_t arg_size,
                     void (*arg_init)(void*, const int, const void*),
                     const size_t env_size, const void *env,
                     hpx_addr_t sync) {
  dbg_assert(max - min > 0);
  dbg_assert(branching_factor > 0);
  dbg_assert(cutoff > 0);

  const int n = max - min;

  // if we're still doing divide and conquer, then do it
  if (n > cutoff) {
    const int m = n / branching_factor;
    int r = n % branching_factor;

    // we'll reuse this buffer
    par_call_async_args_t *args = malloc(sizeof(par_call_async_args_t) + env_size);
    args->action = action;
    args->min = min;
    args->max = min + m + ((r-- > 0) ? 1 : 0);
    args->cutoff = cutoff;
    args->branching_factor = branching_factor;
    args->arg_size = arg_size;
    args->arg_init = arg_init;
    args->sync = sync;
    memcpy(&args->env, env, env_size);

    for (int i = 0, e = branching_factor; i < e; ++i) {
      int e = hpx_call(HPX_HERE, _par_call_async, HPX_NULL, args, sizeof(*args) + env_size);
      if (e) {
        return e;
      }

      // update the buffer to resend
      args->min = args->max;
      args->max = args->max + m + ((r-- > 0) ? 1 : 0);

      if (args->max <= args->min) {
        return HPX_SUCCESS;
      }
    }
  }
  else {
    // otherwise we're in the cutoff region, do the actions sequentially
    for (int i = min, e = max; i < e; ++i) {
      hpx_parcel_t *p = hpx_parcel_acquire(NULL, arg_size);
      hpx_parcel_set_action(p, action);
      hpx_parcel_set_cont_action(p, hpx_lco_set_action);
      hpx_parcel_set_cont_target(p, sync);
      if (arg_init) {
        arg_init(hpx_parcel_get_data(p), i, env);
      }
      hpx_parcel_send(p, HPX_NULL);
    }
  }
  return HPX_SUCCESS;
}

int hpx_par_call(hpx_action_t action, const int min, const int max,
                 const int branching_factor,
                 const int cutoff,
                 const size_t arg_size,
                 void (*arg_init)(void*, const int, const void*),
                 const size_t env_size, const void *env,
                 hpx_addr_t sync) {
  dbg_assert(max - min > 0);
  dbg_assert(branching_factor > 0);
  dbg_assert(cutoff > 0);

  hpx_addr_t and = HPX_NULL;
  if (sync) {
    and = hpx_lco_and_new(max - min);
    hpx_call_when_with_continuation(and, sync, hpx_lco_set_action,
                                    and, hpx_lco_delete_action, NULL, 0);
  }
  return _hpx_par_call_helper(action, min, max, branching_factor, cutoff,
                              arg_size, arg_init, env_size, env, and);
}

int hpx_par_call_sync(hpx_action_t action,
                      const int min, const int max,
                      const int branching_factor,
                      const int cutoff,
                      const size_t arg_size,
                      void (*arg_init)(void*, const int, const void*),
                      const size_t env_size, const void *env) {
  assert(max - min > 0);
  hpx_addr_t sync = hpx_lco_future_new(0);
  if (sync == HPX_NULL) {
    return log_error("could not allocate an LCO.\n");
  }

  int e = hpx_par_call(action, min, max, branching_factor, cutoff, arg_size,
                       arg_init, env_size, env, sync);
  if (!e) {
    e = hpx_lco_wait(sync);
  }
  hpx_lco_delete(sync, HPX_NULL);
  return e;
}
