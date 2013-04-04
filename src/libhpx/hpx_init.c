
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library initialization and cleanup functions
  hpx_init.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/

#include "hpx_init.h"
#include "hpx_ctx.h"
#include "hpx_kthread.h"


/*
 --------------------------------------------------------------------
  hpx_init

  Initializes data structures used by libhpx.  This function must
  be called BEFORE any other functions in libhpx.  Not doing so 
  will cause all other functions to return HPX_ERROR_NOINIT.
 --------------------------------------------------------------------
*/

hpx_error_t hpx_init(void) {
  /* init hpx_errno */
  __hpx_errno = HPX_SUCCESS;

  /* init the next context ID */
  __ctx_next_id = 0;

  /* get the global machine configuration */
  __mcfg = hpx_mconfig_get();

  /* initialize kernel threads */
  //_hpx_kthread_init();

  return HPX_SUCCESS;
}


/*
 --------------------------------------------------------------------
  hpx_cleanup

  Cleans up data structures created by hpx_init.  This function
  must be called after all other HPX functions.
 --------------------------------------------------------------------
*/

void hpx_cleanup(void) {

}
