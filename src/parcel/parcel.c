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
  hpx_action_register

  Register an HPX action.

  Note: ADK: Right now, a parcel handle captures both, an action and
  its continuation, so that it is easier to reuse a parcel handle by
  simply varying the target address and arguments payload.


  -------------------------------------------------------------------
*/
int hpx_new_parcel(char *aname, hpx_action_t act, hpx_action_t cont,
                   hpx_parcel_t *handle) {
}
