// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "libhpx/btt.h"
#include "libhpx/debug.h"

btt_class_t *btt_new(hpx_gas_t type) {
  switch (type) {
   default:
    dbg_log("HPX GAS defaults to PGAS.\n");
   case (HPX_GAS_PGAS):
    return btt_static_new();
   case (HPX_GAS_AGAS):
    return btt_dynamic_new();
  };
}
