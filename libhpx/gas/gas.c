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

#include "libhpx/gas.h"
#include "libhpx/debug.h"

gas_class_t *gas_new(size_t heap_size, struct boot_class *boot,
                     struct transport_class *transport, hpx_gas_t type)
{
  gas_class_t *gas = NULL;
  switch (type) {
   default:
   case (HPX_GAS_DEFAULT):
    dbg_log_gas("HPX GAS defaults to PGAS.\n");
   case (HPX_GAS_PGAS):
   case (HPX_GAS_PGAS_SWITCH):
    gas = gas_pgas_new(heap_size, boot, transport);
    break;
   case (HPX_GAS_AGAS):
    gas = NULL;//gas_agas_new(heap_size);
    break;
   case (HPX_GAS_AGAS_SWITCH):
    gas = NULL;//gas_agas_switch_new(heap_size);
    break;
   case (HPX_GAS_SMP):
    gas = gas_smp_new(heap_size, boot, transport);
    break;
  };
  assert(gas);
  gas->type = type;
  return gas;
}
