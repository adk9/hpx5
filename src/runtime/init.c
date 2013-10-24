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

#include "bootstrap/bootstrap.h"
#include "thread/ctx.h"                         /* libhpx_ctx_init(); */
#include "thread/thread.h"                      /* libhpx_thread_init() */
#include "network/network.h"
#include "parcel/parcelhandler.h"               /* __hpx_parcelhandler */
#include "hpx/error.h"
#include "hpx/init.h"
#include "hpx/parcel.h"
#include "hpx/utils/timer.h"
#include "hpx/thread/ctx.h"

hpx_mconfig_t __mcfg;
hpx_config_t *__hpx_global_cfg = NULL;
network_ops_t *__hpx_network_ops = NULL;
hpx_parcelhandler_t *__hpx_parcelhandler = NULL;
bootstrap_ops_t *bootmgr = NULL;

/**
 * Initializes data structures used by libhpx.  This function must
 * be called BEFORE any other functions in libhpx.  Not doing so
 * will cause all other functions to return HPX_ERROR_NOINIT.
 *
 * @return error code.
 */
hpx_error_t hpx_init(void) {
  __hpx_global_cfg = NULL;

  /* init hpx_errno */
  hpx_error_t success = __hpx_errno = HPX_SUCCESS;

  /* init the next context ID */
  libhpx_ctx_init();

  /* init the thread */
  libhpx_thread_init();

  /* get the global machine configuration */
  __mcfg = hpx_mconfig_get();

  /* initialize kernel threads */
  //_hpx_kthread_init();

  __hpx_global_cfg = hpx_alloc(sizeof(*__hpx_global_cfg));
  if (!__hpx_global_cfg)
    return __hpx_errno;
    
  hpx_config_init(__hpx_global_cfg);
  hpx_config_set_cores(__hpx_global_cfg, 8);

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
  __hpx_parcelhandler = NULL;
#if HAVE_NETWORK
  __hpx_parcelhandler = hpx_parcelhandler_create(__hpx_global_ctx);
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

  if (__hpx_parcelhandler)
    hpx_parcelhandler_destroy(__hpx_parcelhandler);

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
