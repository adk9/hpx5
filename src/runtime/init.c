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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "init.h"                               /* libhpx initializers */
#include "hpx/init.h"                           /* hpx_init(), hpx_cleanup() */
#include "hpx/globals.h"
#include "hpx/error.h"
#include "hpx/parcel.h"
#include "hpx/utils/timer.h"
#include "hpx/thread/ctx.h"
#include "bootstrap.h"                          /* struct bootstrap_ops */
#include "network.h"
#include "parcelhandler.h"                      /* struct parcelhandler */


/**
 * There's one parcelhandler per (UNIX) process at this point.
 */
static struct parcelhandler *the_parcelhandler = NULL;

/**
 * Initializes data structures used by libhpx.
 *
 * This function must be called BEFORE any other functions in libhpx. Not doing
 * so will cause all other functions to return HPX_ERROR_NOINIT.
 *
 * @return error code.
 */
hpx_error_t
hpx_init(void)
{
  /* init hpx_errno */
  hpx_error_t success = HPX_SUCCESS;
  __hpx_errno         = HPX_SUCCESS;

  libhpx_ctx_init();
  libhpx_thread_init();

  /* get the global machine configuration */
  __mcfg = hpx_mconfig_get();

  /* initialize kernel threads */
  //_hpx_kthread_init();

  __hpx_global_cfg = hpx_alloc(sizeof(*__hpx_global_cfg));
  if (!__hpx_global_cfg)
    return __hpx_errno;
    
  hpx_config_init(__hpx_global_cfg);
  //  hpx_config_set_cores(__hpx_global_cfg, 8);

  if(getenv("HPX_NUM_CORES") != NULL) {
    int num_cores;
    num_cores = atoi(getenv("HPX_NUM_CORES"));
    hpx_config_set_cores(__hpx_global_cfg, num_cores);
  }

  __hpx_global_ctx = hpx_ctx_create(__hpx_global_cfg);
  if (!__hpx_global_ctx)
    return __hpx_errno;
  
  /* initialize network */
  __hpx_network_ops = hpx_alloc(sizeof(*__hpx_network_ops));
  *__hpx_network_ops = default_net_ops;
#if HAVE_NETWORK
#if HAVE_PHOTON
  *__hpx_network_ops = photon_ops;
#elif HAVE_MPI
  *__hpx_network_ops = mpi_ops;
#endif

  bootmgr = hpx_alloc(sizeof(*bootmgr));
#if HAVE_MPI
  *bootmgr = mpi_boot_ops;
#else
  *bootmgr = default_boot_ops;
#endif

  /* bootstrap the runtime */
  success = bootmgr->init();
  if (success != HPX_SUCCESS)
    return __hpx_errno;

  /* initialize timer subsystem */
  hpx_timer_init();

  /* initialize network */
  success = __hpx_network_ops->init();
  if (success != HPX_SUCCESS)
    return __hpx_errno;
#endif

  /* initialize the parcel subsystem */
  hpx_parcel_init();
#if HAVE_NETWORK
  the_parcelhandler = parcelhandler_create(__hpx_global_ctx);
#endif

  return success;
}


/**
 * Cleans up data structures created by hpx_init.  This function
 * must be called after all other HPX functions.
 *
 */
void hpx_cleanup(void) {
  /* shutdown the parcel subsystem */
  //hpx_parcel_fini();

  parcelhandler_destroy(the_parcelhandler); /* NULL param ok */

  hpx_ctx_destroy(__hpx_global_ctx); /* note we don't need to free the context - destroy does that */
  hpx_free(__hpx_global_cfg);

  /* finalize the network */
#if HAVE_NETWORK
  __hpx_network_ops->finalize();
  hpx_free(__hpx_network_ops);
#endif

  /* tear down the bootstrap */
  bootmgr->finalize();
  hpx_free(bootmgr);
}
