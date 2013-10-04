/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library initialization and cleanup function definitions
  hpx_init.h

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

#pragma once
#ifndef LIBHPX_INIT_H_
#define LIBHPX_INIT_H_

#include "hpx/bootstrap.h"
#include "hpx/config.h"
#include "hpx/error.h"
#include "hpx/network.h"
#include "hpx/parcelhandler.h"
#include "hpx/thread.h"

/*
 --------------------------------------------------------------------
  Library Globals
 --------------------------------------------------------------------
*/

hpx_config_t *__hpx_global_cfg;
hpx_context_t *__hpx_global_ctx;
hpx_parcelhandler_t *__hpx_parcelhandler;
network_ops_t *__hpx_network_ops;
bootstrap_ops_t *bootmgr;
static hpx_mconfig_t __mcfg;

/*
 --------------------------------------------------------------------
  Initialization & Cleanup Functions
 --------------------------------------------------------------------
*/

hpx_error_t hpx_init(void);
void hpx_cleanup(void);

#endif /* LIBHPX_INIT_H_ */
