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
 * @param func The HPX function that is represented by this action
 * 
 * @return HPX error code
 */
int hpx_action_register(char *name, hpx_func_t func) {
}

int hpx_action_lookup_local(char *name, hpx_func_t *func) {

}

int hpx_action_lookup_byaddr_local(void *, hpx_func_t *func) {

}
