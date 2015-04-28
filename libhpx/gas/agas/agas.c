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
#include <libhpx/boot.h>
#include <libhpx/gas.h>
#include "agas.h"
#include "btt.h"

typedef struct {
  gas_t vtable;
  void *btt;
} agas_t;

static void
_agas_delete(void *gas) {
  agas_t *agas = gas;
  if (agas->btt) {
    btt_delete(agas->btt);
  }
  free(agas);
}

static int64_t
_agas_sub(const void *gas, hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  return INT64_MAX;;
}

static hpx_addr_t
_agas_add(const void *gas, hpx_addr_t gva, int64_t bytes, uint32_t bsize) {
  return HPX_NULL;
}

static hpx_addr_t
_agas_there(void *gas, uint32_t i) {
  return HPX_NULL;
}

static bool
_agas_try_pin(void *gas, hpx_addr_t gpa, void **local) {
  return false;
}

static void
_agas_unpin(void *gas, hpx_addr_t addr) {
}

static gas_t _agas_vtable = {
  .type           = HPX_GAS_AGAS,
  .delete         = _agas_delete,
  .local_size     = NULL,
  .local_base     = NULL,
  .sub            = _agas_sub,
  .add            = _agas_add,
  .there          = _agas_there,
  .try_pin        = _agas_try_pin,
  .unpin          = _agas_unpin,
  .alloc_cyclic   = NULL,
  .calloc_cyclic  = NULL,
  .alloc_blocked  = NULL,
  .calloc_blocked = NULL,
  .alloc_local    = NULL,
  .calloc_local   = NULL,
  .free           = NULL,
  .move           = NULL,
  .memget         = NULL,
  .memput         = NULL,
  .memcpy         = NULL,
  .owner_of       = NULL,
  .mmap           = NULL,
  .munmap         = NULL
};

gas_t *gas_agas_new(const config_t *config, boot_t *boot) {
  agas_t *agas = malloc(sizeof(*agas));
  agas->vtable = _agas_vtable;
  agas->btt = btt_new(0);
  return &agas->vtable;
}
