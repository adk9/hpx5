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
#include "hpx/thread.h"

/*
 --------------------------------------------------------------------
  Library Globals
 --------------------------------------------------------------------
*/

struct hpx_config;
struct hpx_context;
struct network_ops;
struct bootstrap_ops;

extern struct hpx_config *__hpx_global_cfg;
extern struct hpx_context *__hpx_global_ctx;
extern struct network_ops *__hpx_network_ops;
extern struct bootstrap_ops *bootmgr;
extern hpx_mconfig_t __mcfg;

/*
 --------------------------------------------------------------------
  Initialization & Cleanup Functions
 --------------------------------------------------------------------
*/

hpx_error_t hpx_init(void);
void hpx_cleanup(void);

#endif /* LIBHPX_INIT_H_ */
