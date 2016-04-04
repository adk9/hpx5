// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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
#include <libhpx/gas.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>

static int _par_for_async_handler(hpx_for_action_t f, void *args, int min,
                                  int max) {
  for (int i = min, e = max; i < e; ++i) {
    f(i, args);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, 0, _par_for_async, _par_for_async_handler,
                     HPX_POINTER, HPX_POINTER, HPX_INT, HPX_INT);
typedef struct {
  int map_size;
  hpx_addr_t map[];
} _hier_for_map_t;

typedef struct {
  hpx_action_t f;
  hpx_addr_t target;
  hpx_addr_t and;
  int min;
  int max;
  int stride;
  int offset;
  int bsize;
  int arg_size;
  _hier_for_map_t map;
  unsigned char arg[];
} _hier_for_call_args_t;

static int _pinyourself_handler() {
  hpx_addr_t target = hpx_thread_current_target();
  void *local = NULL;
  //we want to pin the addr to avoid the blocks from moving 
  if (!hpx_gas_try_pin(target, (void**)&local)) {
    return HPX_RESEND;
  }
  int rank = HPX_LOCALITY_ID;
  return HPX_THREAD_CONTINUE(rank);
}

static LIBHPX_ACTION(HPX_DEFAULT, 0, _pinyourself, _pinyourself_handler);

/*
 *precompute the distribution of gas on each locality
 */
void precompute_gas_on_each_locality(hpx_addr_t *result, int *sizes,
                                    hpx_addr_t base, int min, int max,
                                    int stride, int offset, int bsize) {
  int n = max - min;
  for (int i = min, e = max; i < e; ++i) {
    // for each block in gas, find the owner of it and classify them into
    hpx_addr_t target = hpx_addr_add(base, i * stride + offset, bsize);
    int rank = 0;
    hpx_call_sync(target, _pinyourself, &rank, sizeof(rank), HPX_NULL, 0);
    result[rank * n + sizes[rank]] = target;
    sizes[rank]++;
  }
}

/*
 *Unpin handler will do the clean up work including unpin the gas and
 *delete the local lco on each locality
 */
static int _unpin_handler(hpx_addr_t *and, size_t UNUSED) {
  //hpx_addr_t addr = hpx_thread_current_target();
  //(void) hpx_gas_unpin(addr);
  //hpx_lco_wait(and);
  hpx_lco_delete(*and, HPX_NULL);
  hpx_addr_t addr = hpx_thread_current_target();
  (void) hpx_gas_unpin(addr);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _unpin, _unpin_handler, 
                     HPX_POINTER, HPX_SIZE_T);

/*
 *Parallel for handler
 *Perform a parallel for on user handler
 */
static HPX_ACTION_DECL(_nested_par_for_async);
static int _hier_par_for_async_handler(_hier_for_call_args_t *args,
                                       size_t UNUSED) {
  for (int i = args->min, e = args->max; i < e; ++i) {
    hpx_addr_t target = args->map.map[i];
    void *local = NULL;
    if (!hpx_gas_try_pin(target, (void**)&local)) {

      // Account for already-processed indices and create a new call for
      //        that subset.
      args->min = i;
      return HPX_RESEND;
    }
    //after user action is done, unpin
    hpx_addr_t and = hpx_lco_and_new(1);
    hpx_call_with_continuation(target, args->f, and, hpx_lco_set_action, 
                               args->arg_size, args->arg);
    hpx_call_when_with_continuation(and, target, _unpin, args->and, 
                                    hpx_lco_set_action, &and, sizeof(and)); 
  }
  return HPX_SUCCESS;
}

static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _hier_par_for_async,
                     _hier_par_for_async_handler, HPX_POINTER, HPX_SIZE_T);

/*
 *This handler distributes all the jobs residing on the current locality
 *among all threads
 */
static int _hier_for_async_handler(_hier_for_call_args_t *args, 
                                   size_t args_size) {
  int nthreads = HPX_THREADS;

  //distributed the work using _nested_par_for
  const int n = args->map.map_size;
  const int m = n / nthreads;
  int r = n % nthreads;

  int base = 0;
  for (int i = 0, e = nthreads; i < e; ++i) {
    args->min = base;
    args->max = base + m + ((r-- > 0) ? 1 : 0);
    base = args->max;
    hpx_call(HPX_HERE, _hier_par_for_async, HPX_NULL, args, args_size);
  }
  return HPX_SUCCESS;
}

static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED,
                     _hier_for_async, _hier_for_async_handler,
                     HPX_POINTER, HPX_SIZE_T);

int hpx_par_for(hpx_for_action_t f, int min, int max, void *args,
                hpx_addr_t sync) {
  dbg_assert(max - min > 0);

  // get the number of scheduler threads
  int nthreads = HPX_THREADS;

  hpx_addr_t and = HPX_NULL;
  hpx_action_t set = hpx_lco_set_action;
  hpx_action_t del = hpx_lco_delete_action;
  if (sync) {
    and = hpx_lco_and_new(nthreads);
    hpx_call_when_with_continuation(and, sync, set, and, del, NULL, 0);
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
    hpx_parcel_t *p = action_new_parcel(_par_for_async, HPX_HERE, and, set,
                                        4, &f, &args, &rmin, &rmax);
    parcel_prepare(p);
    scheduler_spawn_at(p, i);
  }

  return HPX_SUCCESS;
}

int hpx_par_for_sync(hpx_for_action_t f, int min, int max, void *args) {
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

/*
 *Hierarchical for
 *Use an and gate to ensure all elements are visited.
 *Percompute distribution on each locality
 *Distribute jobs among all localities evenly
 */ 	
int hpx_hier_for(hpx_action_t f, int min, int max, int bsize, 
                 int offset, int stride, int arg_size,
                 void *args, hpx_addr_t addr, hpx_addr_t sync) {
  dbg_assert(0 < max - min);
  dbg_assert(0 < stride);
  dbg_assert(0 <= offset);
  dbg_assert(0 < bsize);
  dbg_assert(0 <= arg_size);

  // get the number of scheduler threads
  int nlocalities = HPX_LOCALITIES;

  // sychronization, when all localities are executed "nlocalities" times, end
  hpx_addr_t and = HPX_NULL;
  hpx_action_t set = hpx_lco_set_action;
  hpx_action_t del = hpx_lco_delete_action;
  if (sync) {
    and = hpx_lco_and_new(max - min);
    hpx_call_when_with_continuation(and, sync, set, and, del, NULL, 0);
  }

  // every locality will have an hpx_addr_t array with size "size"

  // compute the range
  int n = max - min;

  // preallocate all of the addresses we're going to send to.
  hpx_addr_t *map = calloc(nlocalities * n, sizeof(*map));
  int *sizes = calloc(nlocalities, sizeof(*sizes));

  precompute_gas_on_each_locality(map, sizes, addr, min, max, stride, offset, 
                                  bsize);
 
  size_t map_size = n * sizeof(*map);
  _hier_for_call_args_t *call_args = malloc(sizeof(*call_args) + arg_size 
                                            + map_size);
  memcpy(&call_args->arg, args, arg_size);
  call_args->and = and;
  call_args->f = f;
  //min and max are not useful at locality level
  call_args->min = 0;
  call_args->max = 0;
  call_args->stride = stride;
  call_args->offset = offset;
  call_args->bsize = bsize;
  call_args->arg_size = arg_size;
  size_t len = sizeof(*call_args) + arg_size + map_size;
  for (int i = 0, e = nlocalities; i < e; ++i) {
    //distribute the block address to the locality owns them
    call_args->map.map_size = sizes[i];
    memcpy(&(call_args->map.map), &(map[i * n]), map_size);
    hpx_call(HPX_THERE(i), _hier_for_async, HPX_NULL, call_args, len);
  }
  (void) free(map);
  (void) free(sizes);
  (void) free(call_args);
  return HPX_SUCCESS;
}

int hpx_hier_for_sync(hpx_action_t f, int min, int max,
                      int bsize, int offset, int stride, int arg_size,
                      void *args, hpx_addr_t addr) {
  dbg_assert(0 < max - min);
  dbg_assert(0 < stride);
  dbg_assert(0 <= offset);
  dbg_assert(0 < bsize);
  dbg_assert(0 <= arg_size);

  hpx_addr_t sync = hpx_lco_future_new(0);
  if (HPX_NULL == sync) {
    return log_error("could not allocate an LCO.\n");
  }

  int e = hpx_hier_for(f, min, max, bsize, offset, stride, arg_size,
                       args, addr, sync);
  if (!e) {
    e = hpx_lco_wait(sync);
  }
  hpx_lco_delete(sync, HPX_NULL);
  return e;
}

/// HPX parallel "call".
/// @struct par_call_async_args_t
/// @brief HPX parallel "call".
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

typedef struct {
  hpx_action_t action;
  hpx_addr_t addr;
  size_t count;
  size_t increment;
  size_t bsize;
  size_t arg_size;
  char arg[];
} hpx_count_range_call_args_t;

static int
_hpx_count_range_call_handler(const hpx_count_range_call_args_t *const args, size_t n) {
  int status;
  for (size_t i = 0; i < args->count; ++i) {
    const hpx_addr_t target =
      hpx_addr_add(args->addr, i * args->increment, args->bsize);
    status = hpx_call(target, args->action, HPX_NULL, args->arg, args->arg_size);
    if (status != HPX_SUCCESS) {
      return status;
    }
  }

  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _hpx_count_range_call,
                     _hpx_count_range_call_handler, HPX_POINTER, HPX_SIZE_T);

int hpx_count_range_call(hpx_action_t action,
                         const hpx_addr_t addr,
                         const size_t count,
                         const size_t increment,
                         const uint32_t bsize,
                         const size_t arg_size,
                         void *const arg) {
  const size_t thread_chunk = count / (HPX_LOCALITIES * HPX_THREADS);
  hpx_count_range_call_args_t *args = malloc(sizeof(*args) + arg_size);
  memcpy(args->arg, arg, arg_size);
  args->action = action; args->count = thread_chunk;
  args->increment = increment; args->bsize = bsize; args->arg_size = arg_size;
  for (size_t l = 0; l < HPX_LOCALITIES; ++l) {
    for (size_t t = 0; t < HPX_THREADS; ++t) {
      const uint64_t addr_delta =
        (l * HPX_THREADS + t) * thread_chunk * increment;
      args->addr = hpx_addr_add(addr, addr_delta, bsize);
      hpx_call(HPX_THERE(l), _hpx_count_range_call, HPX_NULL, args,
               sizeof(*args) + arg_size);
    }
  }
  args->count = count % (HPX_LOCALITIES * HPX_THREADS);
  const uint64_t addr_delta =
    HPX_LOCALITIES * HPX_THREADS * thread_chunk * increment;
  args->addr = hpx_addr_add(addr, addr_delta, bsize);
  hpx_call(HPX_HERE, _hpx_count_range_call, HPX_NULL, args,
           sizeof(*args) + arg_size);
  free(args);
  return HPX_SUCCESS;
}
