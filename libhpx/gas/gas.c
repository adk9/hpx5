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
#include "agas/agas.h"
#include "smp/smp.h"
#include "pgas/pgas.h"

gas_t *gas_new(const config_t *cfg, struct boot *boot) {
  hpx_gas_t type = cfg->gas;
  gas_t *gas = NULL;

  if (boot_n_ranks(boot) == 1) {
    if (type != HPX_GAS_SMP) {
      log_level(HPX_LOG_CONFIG | HPX_LOG_GAS,
                "GAS %s selection override to SMP.\n", HPX_GAS_TO_STRING[type]);
    }
    type = HPX_GAS_SMP;
  }

  if (boot_n_ranks(boot) > 1) {
    if (type == HPX_GAS_SMP) {
      log_level(HPX_LOG_CONFIG | HPX_LOG_GAS,
                "GAS %s selection override to PGAS.\n", HPX_GAS_TO_STRING[type]);
      type = HPX_GAS_PGAS;
    }
  }

  if (type == HPX_GAS_SMP) {
    gas = gas_smp_new();
  }
#ifdef HAVE_NETWORK
  else if (type == HPX_GAS_AGAS) {
    gas = gas_agas_new(cfg, boot);
  }
  else if (type == HPX_GAS_PGAS) {
    gas = gas_pgas_new(cfg, boot);
  }
#endif
  else {
    dbg_error("unexpected configuration value for gas\n");
  }

  if (!gas) {
    log_error("GAS %s failed to initialize\n", HPX_GAS_TO_STRING[type]);
  }
  else {
    log_gas("GAS %s initialized\n", HPX_GAS_TO_STRING[type]);
  }

  return gas;
}
