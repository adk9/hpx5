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

#include "hpx/globals.h"

hpx_mconfig_t __mcfg;
struct hpx_context       *__hpx_global_ctx    = NULL;
struct hpx_config        *__hpx_global_cfg    = NULL;
struct network_ops       *__hpx_network_ops   = NULL;
struct hpx_parcelhandler *__hpx_parcelhandler = NULL;
struct bootstrap_ops     *bootmgr             = NULL;
