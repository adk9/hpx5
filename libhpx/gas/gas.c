// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include "smp/smp.h"
#include "pgas/pgas.h"

gas_t *gas_new(const config_t *cfg, struct boot *boot) {
  hpx_gas_t type = cfg->gas;
  gas_t *gas = NULL;

  if (type != HPX_GAS_PGAS && boot_n_ranks(boot) > 1) {
    log_cfg("GAS %s selection override to PGAS.\n", HPX_GAS_TO_STRING[type]);
  }

  if (type != HPX_GAS_SMP && boot_n_ranks(boot) == 1) {
    log_cfg("GAS %s selection override to SMP.\n", HPX_GAS_TO_STRING[type]);
  }

#ifdef HAVE_NETWORK
  if (boot_n_ranks(boot) > 1) {
    gas = gas_pgas_new(cfg, boot);
    if (!gas) {
      log_gas("PGAS failed to initialize\n");
    }
    else {
      log_gas("PGAS initialized\n");
      gas->type = HPX_GAS_PGAS;
    }
  }
#endif

  if (boot_n_ranks(boot) == 1) {
    gas = gas_smp_new();
    if (!gas) {
      log_gas("SMP failed to initialize\n");
    }
    else {
      log_gas("SMP initialized\n");
      gas->type = HPX_GAS_SMP;
    }
  }

  if (gas) {
    return gas;
  }

  dbg_error("Could not initialize GAS model %s", HPX_GAS_TO_STRING[type]);
  return NULL;
}
