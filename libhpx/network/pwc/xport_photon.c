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
#include <libsync/locks.h>
#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/memory.h>
#include <libhpx/padding.h>
#include <libhpx/system.h>
#include "xport.h"

typedef struct {
  pwc_xport_t  vtable;
  PAD_TO_CACHELINE(sizeof(pwc_xport_t));
} photon_pwc_xport_t;

static void _init_photon_config(const config_t *cfg, boot_t *boot,
                                struct photon_config_t *pcfg) {
  pcfg->meta_exch               = PHOTON_EXCH_EXTERNAL;
  pcfg->nproc                   = boot_n_ranks(boot);
  pcfg->address                 = boot_rank(boot);
  pcfg->comm                    = NULL;
  pcfg->ibv.use_cma             = cfg->photon_usecma;
  pcfg->ibv.eth_dev             = cfg->photon_ethdev;
  pcfg->ibv.ib_dev              = cfg->photon_ibdev;
  pcfg->ugni.eth_dev            = cfg->photon_ethdev;
  pcfg->cap.eager_buf_size      = cfg->photon_eagerbufsize;
  pcfg->cap.small_pwc_size      = cfg->photon_smallpwcsize;
  pcfg->cap.ledger_entries      = cfg->photon_ledgersize;
  pcfg->cap.max_rd              = cfg->photon_maxrd;
  pcfg->cap.default_rd          = cfg->photon_defaultrd;
  pcfg->cap.num_cq              = cfg->photon_numcq;
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

static int _photon_pin(void *xport, void *base, size_t n, void *key) {
  if (PHOTON_OK != photon_register_buffer(base, n)) {
    dbg_error("failed to register segment with Photon\n");
  }
  else {
    log_net("registered segment (%p, %zu)\n", base, n);
  }
  
  const struct photon_buffer_priv_t *bkey;
  if (key) {
    if (PHOTON_OK != photon_get_buffer_private(base, n, &bkey)) {
      dbg_error("failed to get segment key from Photon\n");
    }
    memcpy(key, bkey, sizeof(*bkey));
  }
  return LIBHPX_OK;
}

static int _photon_unpin(void *xport, void *base, size_t n) {
  int e = photon_unregister_buffer(base, n);
  if (PHOTON_OK != e) {
    dbg_error("unhandled error %d during release of segment (%p, %zu)\n", e,
              base, n);
  }
  log_net("released the segment (%p, %zu)\n", base, n);
  return LIBHPX_OK;
}

static int _photon_pwc(int r, void *rva, const void *rolva, size_t n,
                       uint64_t lsync, uint64_t rsync, void *rkey) {
  int flag = ((lsync) ? 0 : PHOTON_REQ_PWC_NO_LCE) |
             ((rsync) ? 0 : PHOTON_REQ_PWC_NO_RCE);

  struct photon_buffer_t lbuf, rbuf;
  rbuf.addr = (uintptr_t)rva;
  rbuf.size = n;
  rbuf.priv = *(struct photon_buffer_priv_t*)rkey;
  
  lbuf.addr = (uintptr_t)rolva;
  lbuf.size = n;
  lbuf.priv = (struct photon_buffer_priv_t){0,0};
  
  int e = photon_put_with_completion(r, n, &lbuf, &rbuf, lsync, rsync, flag);
  switch (e) {
   case PHOTON_OK:
    return LIBHPX_OK;
   case PHOTON_ERROR_RESOURCE:
    return LIBHPX_RETRY;
   default:
    dbg_error("could not initiate a put-with-completion\n");
  }
}

static int _photon_gwc(int r, void *lva, const void *rorva, size_t n,
                       uint64_t lsync, void *rkey) {

  photon_rid rsync = 0;
  struct photon_buffer_t lbuf, rbuf;
  rbuf.addr = (uintptr_t)rorva;
  rbuf.size = n;
  rbuf.priv = *(struct photon_buffer_priv_t*)rkey;
  
  lbuf.addr = (uintptr_t)lva;
  lbuf.size = n;
  lbuf.priv = (struct photon_buffer_priv_t){0,0};

  int e = photon_get_with_completion(r, n, &lbuf, &rbuf, lsync, rsync,
				     PHOTON_REQ_PWC_NO_RCE);
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

pwc_xport_t *pwc_xport_new_photon(const config_t *cfg, boot_t *boot, gas_t *gas)
{
  photon_pwc_xport_t *photon = malloc(sizeof(*photon));
  dbg_assert(photon);
  _init_photon(cfg, boot);

  photon->vtable.type = HPX_TRANSPORT_PHOTON;
  photon->vtable.delete = _photon_delete;
  photon->vtable.sizeof_rdma_key = _photon_sizeof_rdma_key;
  photon->vtable.clear = _photon_clear;
  photon->vtable.pin = _photon_pin;
  photon->vtable.unpin = _photon_unpin;
  photon->vtable.pwc = _photon_pwc;
  photon->vtable.gwc = _photon_gwc;
  photon->vtable.test = _photon_test;
  photon->vtable.probe = _photon_probe;

  local = address_space_new_default(cfg);
  registered = address_space_new_jemalloc_registered(cfg, photon, _photon_pin,
                                                     _photon_unpin, NULL,
                                                     system_mmap_huge_pages,
                                                     system_munmap);
  global = address_space_new_jemalloc_global(cfg, photon, _photon_pin,
                                             _photon_unpin, gas, gas_mmap,
                                             gas_munmap);

  return &photon->vtable;
}
