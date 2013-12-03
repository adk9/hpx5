/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Predefined Actions
  predefined.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/
#include "hpx/error.h"
#include "hpx/lco.h"
#include "hpx/mem.h"                            /*  */
#include "hpx/runtime.h"                        /* hpx_get_num_localities() */
#include "predefined_actions.h"

hpx_action_t action_set_shutdown_future = HPX_ACTION_NULL;
hpx_future_t *shutdown_futures = NULL;

static hpx_error_t init_shutdown_futures();

static void set_shutdown_future(void* arg);

/**
 * Allocate and initialize an array of n futures.
 */
static hpx_error_t alloc_and_init_futures(hpx_future_t **fut_arr, unsigned n) {
  unsigned i;
  hpx_future_t *futs = hpx_alloc(sizeof(*futs) * n);
  if (futs == NULL)
    return (__hpx_errno = HPX_ERROR_NOMEM);
  for (i = 0; i < n; i++)
    hpx_lco_future_init(&(futs[i]));

  *fut_arr = futs;

  return HPX_SUCCESS;
}

hpx_error_t init_predefined() {
  hpx_error_t ret;

  ret = init_shutdown_futures();
  if (ret != HPX_SUCCESS)
    return ret;

  action_set_shutdown_future = hpx_action_register("set_shutdown_future", set_shutdown_future);    

  return HPX_SUCCESS;
}

/*
 * Initialize shutdown_futures used by set_shutdown_future (and action action_set_shutdown_future)
 */
hpx_error_t init_shutdown_futures() {
  unsigned int num_processes = hpx_get_num_localities(); /* TODO: Once we can add dynamic processes we will need to change this BDM */
  return alloc_and_init_futures(&shutdown_futures, num_processes);
}

/*
 * Set the shutdown_futures for rank i (to be invoked by rank i at all other ranks). Takes an argument of type struct {size_t rank}
 */
void set_shutdown_future(void* voidp_arg) {
  struct {size_t rank;} *arg = voidp_arg;
  size_t rank = arg->rank;
  hpx_lco_future_set_state(&shutdown_futures[rank]);
}
