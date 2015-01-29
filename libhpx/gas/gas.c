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
                     struct transport_class *transport, hpx_gas_t type) {
  gas_class_t *gas = NULL;

  if (type == HPX_GAS_DEFAULT) {
    log_gas("HPX GAS defaults to PGAS.\n");
  }

  if (type == HPX_GAS_PGAS) {
    gas = gas_pgas_new(heap_size, boot, transport);
    if (!gas) {
      log_gas("PGAS failed to initialize\n");
    }
    else {
      log_gas("PGAS initialized\n");
      gas->type = HPX_GAS_PGAS;
    }
  }

  if (type == HPX_GAS_SMP || !gas) {
    gas = gas_smp_new(heap_size, boot, transport);
    if (!gas) {
      log_gas("SMP failed to initialize\n");
    }
    else {
      log_gas("SMP initialized\n");
      gas->type = HPX_GAS_SMP;
    }
  }

  if (gas)
    return gas;

  dbg_error("Could not initialize GAS model %s", HPX_GAS_TO_STRING[type]);
  return NULL;
}
