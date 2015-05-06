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
#include <libhpx/gpa.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/padding.h>
#include <libhpx/system.h>
#include "registered.h"
#include "xport.h"
#include "../commands.h"

// check to make sure we can fit a photon key in the key size
_HPX_ASSERT(XPORT_KEY_SIZE == sizeof(struct photon_buffer_priv_t),
            incompatible_key_size);

typedef struct {
  pwc_xport_t  vtable;
  PAD_TO_CACHELINE(sizeof(pwc_xport_t));
} photon_pwc_xport_t;

static void
_init_photon_config(const config_t *cfg, boot_t *boot,
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

static void
_init_photon(const config_t *cfg, boot_t *boot) {
  if (photon_initialized()) {
    return;
  }

  struct photon_config_t pcfg;
  _init_photon_config(cfg, boot, &pcfg);
  if (photon_init(&pcfg) != PHOTON_OK) {
    dbg_error("failed to initialize transport.\n");
  }
}

static void
_photon_key_clear(void *key) {
  memset(key, 0, sizeof(struct photon_buffer_priv_t));
}

static void
_photon_key_copy(void *restrict dest, const void *restrict src) {
  if (src) {
    dbg_assert(dest);
    memcpy(dest, src, sizeof(struct photon_buffer_priv_t));
  }
}

static const void *
_photon_key_find_ref(void *obj, const void *addr, size_t n) {
  const struct photon_buffer_priv_t *found = NULL;
  int e = photon_get_buffer_private((void*)addr, n, &found);
  if (PHOTON_OK != e) {
    dbg_assert(found == NULL);
    log_net("no rdma key for range (%p, %zu)\n", addr, n);
  }
  return found;
}

static void
_photon_key_find(void *obj, const void *addr, size_t n, void *key) {
  const void *found = _photon_key_find_ref(obj, addr, n);
  if (found)  {
    _photon_key_copy(key, found);
  }
  else {
    dbg_error("failed to find rdma key for (%p, %zu)\n", addr, n);
  }
}

static void
_photon_pin(const void *base, size_t n, void *key) {
  if (PHOTON_OK != photon_register_buffer((void*)base, n)) {
    dbg_error("failed to register segment with Photon\n");
  }
  else {
    log_net("registered segment (%p, %zu)\n", base, n);
  }

  if (key) {
    _photon_key_find(NULL, base, n, key);
  }
}

static void
_photon_unpin(const void *base, size_t n) {
  int e = photon_unregister_buffer((void*)base, n);
  if (PHOTON_OK != e) {
    dbg_error("unhandled error %d during release of segment (%p, %zu)\n", e,
              base, n);
  }
  log_net("released the segment (%p, %zu)\n", base, n);

}

// async entry point for unpin
static int
_photon_unpin_async(const void *base, size_t n) {
  _photon_unpin(base, n);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_INTERRUPT, 0, _unpin_async, _photon_unpin_async,
                  HPX_POINTER, HPX_SIZE_T);

static command_t
_chain_unpin(const void *addr, size_t n, command_t op) {
  hpx_addr_t lsync = hpx_lco_future_new(0);
  hpx_call_when_with_continuation(lsync, HPX_HERE, _unpin_async, lsync,
                                  hpx_lco_delete_action, &addr, &n);
  if (op) {
    int rank = here->rank;
    hpx_action_t lop = command_get_op(op);
    hpx_addr_t laddr = offset_to_gpa(rank, command_get_arg(op));
    hpx_call_when_with_continuation(lsync, laddr, lop, 0, 0, &rank, &laddr);
  }

  return command_pack(lco_set, lsync);
}

static int
_photon_command(const xport_op_t *op) {
  int flags = ((op->lop) ? 0 : PHOTON_REQ_PWC_NO_LCE) |
              ((op->rop) ? 0 : PHOTON_REQ_PWC_NO_RCE);

  int e = photon_put_with_completion(op->rank, 0, NULL, NULL, op->lop, op->rop,
                                     flags);
  if (PHOTON_OK == e) {
    return LIBHPX_OK;
  }

  if (PHOTON_ERROR_RESOURCE == e) {
    log_error("could not initiate command due to resource constraint\n");
    return LIBHPX_RETRY;
  }

  dbg_error("could not initiate a put-with-completion\n");
}

static int
_photon_pwc(xport_op_t *op) {
  int flags = ((op->lop) ? 0 : PHOTON_REQ_PWC_NO_LCE) |
              ((op->rop) ? 0 : PHOTON_REQ_PWC_NO_RCE);

  struct photon_buffer_t rbuf = {
    .addr = (uintptr_t)op->dest,
    .size = op->n
  };
  _photon_key_copy(&rbuf.priv, op->dest_key);

  struct photon_buffer_t lbuf = {
    .addr = (uintptr_t)op->src,
    .size = op->n
  };

  if (op->src_key) {
    _photon_key_copy(&lbuf.priv, op->src_key);
  }
  else {
    log_net("temporarily registering buffer (%p, %lu)\n", op->src, op->n);
    _photon_pin(op->src, op->n, &lbuf.priv);
    op->lop = _chain_unpin(op->src, op->n, op->lop);
  }

  int e = photon_put_with_completion(op->rank, op->n, &lbuf, &rbuf, op->lop,
                                     op->rop, flags);
  if (PHOTON_OK == e) {
    return LIBHPX_OK;
  }

  if (PHOTON_ERROR_RESOURCE == e) {
    log_error("could not initiate pwc due to resource constraint\n");
    return LIBHPX_RETRY;
  }

  dbg_error("could not initiate a put-with-completion\n");
}

static int
_photon_gwc(xport_op_t *op) {
  int flags = (op->rop) ? 0 : PHOTON_REQ_PWC_NO_RCE;

  struct photon_buffer_t lbuf = {
    .addr = (uintptr_t)op->dest,
    .size = op->n
  };

  if (op->dest_key) {
    _photon_key_copy(&lbuf.priv, op->dest_key);
  }
  else {
    log_net("temporarily registering buffer (%p, %lu)\n", op->dest, op->n);
    _photon_pin(op->dest, op->n, &lbuf.priv);
    op->lop = _chain_unpin(op->dest, op->n, op->lop);
  }

  struct photon_buffer_t rbuf = {
    .addr = (uintptr_t)op->src,
    .size = op->n
  };
  dbg_assert(op->src_key);
  _photon_key_copy(&rbuf.priv, op->src_key);

  int e = photon_get_with_completion(op->rank, op->n, &lbuf, &rbuf, op->lop,
                                     op->rop, flags);
  if (PHOTON_OK == e) {
    return LIBHPX_OK;
  }

  dbg_error("failed transport get operation\n");
}

static int
_poll(uint64_t *op, int *remaining, int src, int type) {
  int flag = 0;
  int e = photon_probe_completion(src, &flag, remaining, op, type);
  if (PHOTON_OK != e) {
    dbg_error("photon probe error\n");
  }
  return flag;
}

static int
_photon_test(uint64_t *op, int *remaining) {
  return _poll(op, remaining, PHOTON_ANY_SOURCE, PHOTON_PROBE_EVQ);
}

static int
_photon_probe(uint64_t *op, int *remaining, int src) {
  return _poll(op, remaining, src, PHOTON_PROBE_LEDGER);
}

static void
_photon_delete(void *photon) {
  free(photon);
}

pwc_xport_t *
pwc_xport_new_photon(const config_t *cfg, boot_t *boot, gas_t *gas) {
  photon_pwc_xport_t *photon = malloc(sizeof(*photon));
  dbg_assert(photon);
  _init_photon(cfg, boot);

  photon->vtable.type = HPX_TRANSPORT_PHOTON;
  photon->vtable.delete = _photon_delete;
  photon->vtable.key_find_ref = _photon_key_find_ref;
  photon->vtable.key_find = _photon_key_find;
  photon->vtable.key_clear = _photon_key_clear;
  photon->vtable.key_copy = _photon_key_copy;
  photon->vtable.pin = _photon_pin;
  photon->vtable.unpin = _photon_unpin;
  photon->vtable.command = _photon_command;
  photon->vtable.pwc = _photon_pwc;
  photon->vtable.gwc = _photon_gwc;
  photon->vtable.test = _photon_test;
  photon->vtable.probe = _photon_probe;

  // initialize the registered memory allocator
  registered_allocator_init(&photon->vtable);
  return &photon->vtable;
}
