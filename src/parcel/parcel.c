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

#define _GNU_SOURCE 
#include <stdlib.h>
#include <search.h>

#include "hpx/action.h"
#include "hpx/comm.h"
#include "hpx/parcel.h"

struct hsearch_data action_table;

int hpx_parcel_init(void) {
    /* create an action table */

    bzero(&action_table, sizeof(action_table));
    /* ADK: TODO: we need a better hash table implementation. */
    hcreate_r(ACTION_TABLE_SIZE, &action_table);

    if (!action_table)
        return HPX_ERROR_NOMEM;

    /* spawn the parcel handler/dispatcher thread */
}


void hpx_parcel_fini(void) {
    /* destroy the action table */
    hdestroy_r(&action_table);

    /* shutdown the parcel handler thread */
    
}

/*
 --------------------------------------------------------------------
  hpx_new_parcel

  Create a new parcel. We pass in an action @act, instead of the
  action's name so that we can save the lookup cost at the remote
  locality if we are running under the SPMD/symmetric heap assumption.
  -------------------------------------------------------------------
*/
int hpx_new_parcel(char *act, hpx_parcel_t *handle) {
}

hpx_thread_t *hpx_call(hpx_locality_t *dest, hpx_action_t *action,
                       void *args, size_t len) {
  int ret;
  hpx_parcel_t p;
  /* create a parcel from action, args, len */
  hpx_new_parcel(action, args, len, &p);
  /* send parcel to the destination locality */
    
  ret = comm_send_parcel(dest, &p);
  return ret;
}
