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

#include <photon.h>
#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include "transport.h"

typedef struct photon_config_t photon_config_t;

typedef struct {
  hpx_transport_t type;
  struct photon_config_t cfg;
} photon_transport_t;

static void _init_photon_config(const config_t *cfg, boot_t *boot,
                                photon_config_t *pcfg) {
  pcfg->meta_exch               = PHOTON_EXCH_EXTERNAL;
  pcfg->nproc                   = boot_n_ranks(boot);
  pcfg->address                 = boot_rank(boot);
  pcfg->comm                    = NULL;
  pcfg->ibv.use_cma             = cfg->photon_usecma;
  pcfg->ibv.eth_dev             = cfg->photon_ethdev;
  pcfg->ibv.ib_dev              = cfg->photon_ibdev;
  pcfg->cap.eager_buf_size      = cfg->photon_eagerbufsize;
  pcfg->cap.small_pwc_size      = cfg->photon_smallpwcsize;
  pcfg->cap.ledger_entries      = cfg->photon_ledgersize;
  pcfg->cap.max_rd              = cfg->photon_maxrd;
  pcfg->cap.default_rd          = cfg->photon_defaultrd;
  // static config not relevant for current HPX usage
  pcfg->forwarder.use_forwarder =  0;
  pcfg->cap.small_msg_size      = -1;  // default 4096 - not used for PWC
  pcfg->ibv.use_ud              =  0;  // don't enable this unless we're doing HW GAS
  pcfg->ibv.ud_gid_prefix       = "ff0e::ffff:0000:0000";
  pcfg->exch.allgather      = (__typeof__(pcfg->exch.allgather))boot->allgather;
  pcfg->exch.barrier        = (__typeof__(pcfg->exch.barrier))boot->barrier;
  pcfg->backend             = (char*)HPX_PHOTON_BACKEND_TO_STRING[cfg->photon_backend];
}

static void _init_photon(photon_config_t *pcfg) {
  if (photon_initialized()) {
    return;
  }

  if (photon_init(pcfg) != PHOTON_OK) {
    dbg_error("failed to initialize transport.\n");
  }
}

void *pwc_transport_new_photon(const config_t *cfg, boot_t *boot) {
  photon_transport_t *photon = malloc(sizeof(*photon));
  dbg_assert(photon);
  photon->type = HPX_TRANSPORT_PHOTON;
  _init_photon_config(cfg, boot, &photon->cfg);
  _init_photon(&photon->cfg);
  return photon;
}
