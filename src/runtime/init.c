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

#include "hpx/init.h"
#include "hpx/parcel.h"
#include "hpx/ctx.h"
#include "hpx/network.h"


/**
 * Initializes data structures used by libhpx.  This function must
 * be called BEFORE any other functions in libhpx.  Not doing so 
 * will cause all other functions to return HPX_ERROR_NOINIT.
 * 
 * @return error code.
 */
hpx_error_t hpx_init(void) {
  /* init hpx_errno */
  __hpx_errno = HPX_SUCCESS;

  /* init the next context ID */
  __ctx_next_id = 1;

  /* init the next thread ID */
  __thread_next_id = 1;

  /* get the global machine configuration */
  __mcfg = hpx_mconfig_get();

  /* initialize kernel threads */
  //_hpx_kthread_init();

  /* initialize network */
  __hpx_network_ops = hpx_alloc(sizeof(network_ops_t));
#if HAVE_MPI
  *__hpx_network_ops = mpi_ops;
  __hpx_network_ops->init();
  
#endif

  /* initialize the parcel subsystem */
  hpx_parcel_init();
  //  __hpx_parcelhandler = hpx_parcelhandler_create();

  /* initialize timer subsystem */
  hpx_timer_init();

  return HPX_SUCCESS;
}


/**
 * Cleans up data structures created by hpx_init.  This function
 * must be called after all other HPX functions.
 * 
 */
void hpx_cleanup(void) {
  /* shutdown the parcel subsystem */
  //hpx_parcel_fini();

  //  hpx_parcelhandler_destroy(__hpx_parcelhandler); 

  /* TODO:
    lookup correct network finalize
    call it
  */
  __hpx_network_ops->finalize();
  hpx_free(__hpx_network_ops);
}
