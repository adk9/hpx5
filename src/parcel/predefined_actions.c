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
#include "hpx/mem.h"
#include "hpx/runtime.h" /* hpx_get_num_localities() */
#include "parcel/predefined_actions.h"

#include <stdio.h>

hpx_error_t init_shutdown_futures();

void set_shutdown_future(void* arg);

static hpx_error_t alloc_and_init_futures(hpx_future_t **fut_arr, unsigned n) {
  unsigned i;
  hpx_future_t *futs = hpx_alloc(sizeof(hpx_future_t)*n);
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

hpx_error_t init_shutdown_futures() {
  unsigned int num_processes = hpx_get_num_localities(); /* TODO: Once we can add dynamic processes we will need to change this BDM */
  return alloc_and_init_futures(&shutdown_futures, num_processes);
}

void set_shutdown_future(void* voidp_arg) {
  //  struct si {size_t rank;}

  //  size_t rank = *(size_t*)arg;
  struct {size_t rank;} *arg = voidp_arg;
  size_t rank = arg->rank;
  hpx_lco_future_set_state(&shutdown_futures[rank]);
  free(voidp_arg);
#if 0
  struct shutdown_parcel_args* args = (struct shutdown_parcel_args*)voidp_args;
  if (hpx_get_rank() != 0)
    hpx_lco_future_set_state(args->ret_fut);
  else {
    unsigned i;
    hpx_lco_future_set_state(parcel_shutdown_futures[args->rank]);
    for (i = 1; i < hpx_get_num_processes; i++)
      thread_wait(parcel_shutdown_futures[i]);
    hpx_parcel_t* p = hpx_alloc(sizeof(hpx_parcel_t));
    if (p == NULL) {
      __hpx_errno = HPX_ERROR_NOMEM;
      return;
    }
    hpx_new_parcel(action_shutdown_parcel, args, sizeof(*args), p); /* should check error but how would we handle it? */
    hpx_send_parcel(hpx_find_locality(args->rank, p)); /* FIXME change hpx_find_locality to something more appropriate when merging with up to date code */
  }
#endif
}
