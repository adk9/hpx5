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

#include "hpx/error.h"
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
  hpx_error_t success;
  __hpx_global_cfg = NULL;
  __hpx_global_ctx = NULL;

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

  __hpx_global_cfg = hpx_alloc(sizeof(hpx_config_t));

  if (__hpx_global_cfg != NULL) {
    hpx_config_init(__hpx_global_cfg);
    __hpx_global_ctx = hpx_ctx_create(__hpx_global_cfg);
    if (__hpx_global_ctx == NULL) {
      __hpx_errno = HPX_ERROR;
      return HPX_ERROR;
    }
  }
  else {
    __hpx_errno = HPX_ERROR_NOMEM;
    return HPX_ERROR_NOMEM;
  }


  /* initialize network */
  __hpx_network_ops = hpx_alloc(sizeof(network_ops_t));
  *__hpx_network_ops = default_ops;
#if HAVE_NETWORK
#if HAVE_PHOTON
  //#warning Building with photon...
  *__hpx_network_ops = photon_ops;
#elif HAVE_MPI
  *__hpx_network_ops = mpi_ops;
#endif
  success = __hpx_network_ops->init();
  if (success != HPX_SUCCESS) {
    __hpx_errno = HPX_ERROR;
    return HPX_ERROR;
  }
#endif

  /* initialize the parcel subsystem */
  hpx_parcel_init();
  __hpx_parcelhandler = NULL;
#if HAVE_NETWORK
  __hpx_parcelhandler = hpx_parcelhandler_create(__hpx_global_ctx);
#endif

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

#if HAVE_NETWORK
  hpx_parcelhandler_destroy(__hpx_parcelhandler); 
#endif

  hpx_ctx_destroy(__hpx_global_ctx); /* note we don't need to free the context - destroy does that */
  hpx_free(__hpx_global_cfg);

#if HAVE_NETWORK
  __hpx_network_ops->finalize();
  hpx_free(__hpx_network_ops);
#endif
}
