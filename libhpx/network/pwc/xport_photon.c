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

#include <stdlib.h>
#include <string.h>
#include <photon.h>
#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/memory.h>
#include <libhpx/system.h>
#include "xport.h"

static void _init_photon_config(const config_t *cfg, boot_t *boot,
                                struct photon_config_t *pcfg) {
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

static void _init_photon(const config_t *cfg, boot_t *boot) {
  if (photon_initialized()) {
    return;
  }

  struct photon_config_t pcfg;
  _init_photon_config(cfg, boot, &pcfg);
  if (photon_init(&pcfg) != PHOTON_OK) {
    dbg_error("failed to initialize transport.\n");
  }
}

static size_t _photon_sizeof_rdma_key(void) {
  return sizeof(struct photon_buffer_priv_t);
}

static void _photon_clear(void *key) {
  memset(key, 0, sizeof(struct photon_buffer_priv_t));
}

static int _photon_pin(void *base, size_t n, void *key) {
  if (PHOTON_OK != photon_register_buffer(base, n)) {
    dbg_error("failed to register segment with Photon\n");
  }
  else {
    log_net("registered segment (%p, %lu)\n", base, n);
  }

  if (key) {
    if (PHOTON_OK != photon_get_buffer_private(base, n, key)) {
      dbg_error("failed to segment key from Photon\n");
    }
  }

  return LIBHPX_OK;
}

static int _photon_unpin(void *base, size_t n) {
  if (PHOTON_OK != photon_unregister_buffer(base, n)) {
    log_net("could not unregister the local heap segment %p\n", base);
    return LIBHPX_ERROR;
  }

  return LIBHPX_OK;
}

static int _photon_pwc(int r, void *rva, const void *rolva, size_t n,
                       uint64_t lsync, uint64_t rsync, void *rkey) {
  int flag = ((lsync) ? 0 : PHOTON_REQ_PWC_NO_LCE) |
             ((rsync) ? 0 : PHOTON_REQ_PWC_NO_RCE);

  struct photon_buffer_priv_t *key = rkey;
  void *lva = (void*)rolva;
  int e = photon_put_with_completion(r, lva, n, rva, *key, lsync, rsync, flag);
  switch (e) {
   case PHOTON_OK:
    return LIBHPX_OK;
   case PHOTON_ERROR_RESOURCE:
    return LIBHPX_RETRY;
   default:
    return log_error("could not initiate a put-with-completion\n");
  }
}

static int _photon_gwc(int r, void *lva, const void *rorva, size_t n,
                       uint64_t lsync, void *rkey) {
  struct photon_buffer_priv_t *key = rkey;
  void *rva = (void*)rorva;
  int e = photon_get_with_completion(r, lva, n, rva, *key, lsync, 0);
  dbg_assert_str(PHOTON_OK == e, "failed transport get operation\n");
  return LIBHPX_OK;

}

static int _poll(uint64_t *op, int *remaining, int src, int type) {
  int flag = 0;
  int e = photon_probe_completion(src, &flag, remaining, op, type);
  if (PHOTON_OK != e) {
    dbg_error("photon probe error\n");
  }
  return flag;
}

static int _photon_test(uint64_t *op, int *remaining) {
  return _poll(op, remaining, PHOTON_ANY_SOURCE, PHOTON_PROBE_EVQ);
}

static int _photon_probe(uint64_t *op, int *remaining, int src) {
  return _poll(op, remaining, src, PHOTON_PROBE_LEDGER);
}

static void _photon_delete(void *photon) {
  free(photon);
}

pwc_xport_t *pwc_xport_new_photon(const config_t *cfg, boot_t *boot) {
  pwc_xport_t *photon = malloc(sizeof(*photon));
  dbg_assert(photon);
  _init_photon(cfg, boot);

  photon->type = HPX_TRANSPORT_PHOTON;
  photon->delete = _photon_delete;
  photon->sizeof_rdma_key = _photon_sizeof_rdma_key;
  photon->clear = _photon_clear;
  photon->pin = _photon_pin;
  photon->unpin = _photon_unpin;
  photon->pwc = _photon_pwc;
  photon->gwc = _photon_gwc;
  photon->test = _photon_test;
  photon->probe = _photon_probe;

  local = address_space_new_default(cfg);
  registered = address_space_new_jemalloc_registered(cfg,
                                                     _photon_pin,
                                                     _photon_unpin,
                                                     system_mmap,
                                                     system_munmap);
  global = address_space_new_jemalloc_global(cfg);

  return photon;
}
