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

  /************************************************************************/
  /* ADK: There are a few ways to handle action registration--The         */
  /* simplest is under the naive assumption that we are executing in a    */
  /* homogeneous, SPMD environment and parcels simply carry function      */
  /* pointers around. The second is to have all interested localities     */
  /* register the required functions and then simply pass tags            */
  /* around. Finally, a simpler, yet practical alternative, is to have a  */
  /* local registration scheme for exported functions. Eventually, we     */
  /* would want to have a distributed namespace for parcels that provides */
  /* all three options.                                                   */
  /************************************************************************/

/** 
 * Register an HPX action.
 * 
 * @param name Action Name
 * @param func The HPX function that is represented by this action.
 * 
 * @return HPX error code
 */
int hpx_action_register(char *name, hpx_func_t func, hpx_action_t *action) {
    int ret;
    hpx_action_t *a;
    ENTRY *e;

    if (action_table == NULL)
        return HPX_ERROR;

    a = hpx_alloc(sizeof(*a));
    a->name = name;
    a->action = func;

    ret = hsearch_r(*(ENTRY*)a, ENTER, &e, &action_table);
    if (e == NULL || ret < 0)
        return HPX_ERROR;

    free(a);
    return HPX_SUCCESS;
}

int hpx_action_lookup_local(char *name, hpx_action_t *action) {
    int ret;
    hpx_action_t *a;
    ENTRY e;

    if (action_table == NULL)
        return HPX_ERROR;

    e.key = name;
    ret = hsearch_r(e, FIND, (ENTRY**)&a, &action_table);
    return a;
}

// reverse lookup
int hpx_action_lookup_addr_local(hpx_func_t *func, hpx_action_t *action) {
}

int hpx_action_invoke(hpx_action_t *action, void *args, void **result) {
    hpx_thread_t *th;
    void *ctx; // TODO
    // spawn a thread to invoke the action locally
    th = hpx_thread_create(ctx, action->action, args);
    hpx_thread_join(th, result);
    return 0;
}
