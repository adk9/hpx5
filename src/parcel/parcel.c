/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Parcel Registration
  register.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#include <stdlib.h>

#include "hpx/action.h"
#include "hpx/parcel.h"

/*
 --------------------------------------------------------------------
  hpx_new_parcel

  Create a new parcel. We pass in an action @act, instead of the
  action's name so that we can save the lookup cost at the remote
  locality if we are running under the SPMD/symmetric heap assumption.
  -------------------------------------------------------------------
*/

static int parcel_send

int hpx_new_parcel(char *act, hpx_parcel_t *handle) {
}

hpx_thread_t *hpx_call(hpx_locality_t *dest, hpx_action_t *action,
                       void *args, size_t len) {
    int ret;
    hpx_parcel_t p;
    /* create a parcel from action, args, len */
    hpx_new_parcel(action, args, len, &p);
    /* send parcel to the destination locality */
    ret = comm_parcel_send(dest, &p);
    return ret;
}
