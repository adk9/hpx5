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

btt_class_t *btt_new(hpx_gas_t type, size_t heap_size) {
  btt_class_t *btt = NULL;
  switch (type) {
   default:
   case (HPX_GAS_DEFAULT):
    dbg_log_gas("HPX GAS defaults to PGAS.\n");
   case (HPX_GAS_PGAS):
   case (HPX_GAS_PGAS_SWITCH):
    btt = btt_pgas_new(heap_size);
    break;
   case (HPX_GAS_AGAS):
    btt = btt_agas_new(heap_size);
    break;
   case (HPX_GAS_AGAS_SWITCH):
    btt = btt_agas_switch_new(heap_size);
    break;
   case (HPX_GAS_SMP):
    btt = btt_local_only_new(heap_size);
    break;
  };
  assert(btt);
  btt->type = type;
  return btt;
}
