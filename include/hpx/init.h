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

#include "hpx/error.h"
#include "hpx/network.h"
#include "hpx/parcelhandler.h"

#ifdef __x86_64__
  #include "arch/x86_64/mconfig.h"
#endif

/*
 --------------------------------------------------------------------
  Library Globals
 --------------------------------------------------------------------
*/

hpx_mconfig_t __mcfg;
hpx_parcelhandler_t *__hpx_parcelhandler;
network_ops_t *__hpx_network_ops;

/*
 --------------------------------------------------------------------
  Initialization & Cleanup Functions
 --------------------------------------------------------------------
*/

hpx_error_t hpx_init(void);
void hpx_cleanup(void);

#endif /* LIBHPX_INIT_H_ */
