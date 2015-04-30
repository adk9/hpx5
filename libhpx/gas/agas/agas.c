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
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include "agas.h"
#include "btt.h"
#include "gva.h"

static const unsigned AGAS_MAX_RANKS = (1u << GVA_RANK_BITS);
static const uint64_t AGAS_THERE_OFFSET = UINT64_MAX;

static uint64_t size_classes[8] = {
  0,
  8,
  64,
  128,
  1024,
  16384,
  65536,
  1048576
};

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
  // We reserve a small range of addresses in the "large" allocation space that
  // will represent locality addresses.
  dbg_assert(i < AGAS_MAX_RANKS);
  dbg_assert(i < here->ranks);

  static const int class = 1;
  uint64_t mask = ~(size_classes[class] - 1);
  gva_t gva = {
    .bits = {
      .offset = AGAS_THERE_OFFSET & mask,
      .home = i,
      .large = 0,
      .size_class = class
    }
  };
  return gva.addr;
}

static bool
_agas_try_pin(void *gas, hpx_addr_t gva, void **lva) {
  agas_t *agas = gas;
  return btt_try_pin(agas->btt, gva, lva);
}

static void
_agas_unpin(void *gas, hpx_addr_t gva) {
  agas_t *agas = gas;
  btt_unpin(agas->btt, gva);
}

static uint32_t
_agas_owner_of(const void *gas, hpx_addr_t gva) {
  const agas_t *agas = gas;
  return btt_owner_of(agas->btt, gva);
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
  .owner_of       = _agas_owner_of,
  .mmap           = NULL,
  .munmap         = NULL
};

gas_t *gas_agas_new(const config_t *config, boot_t *boot) {
  agas_t *agas = malloc(sizeof(*agas));
  agas->vtable = _agas_vtable;
  agas->btt = btt_new(0);
  btt_insert(agas->btt, _agas_there(agas, here->rank), here->rank, here);
  return &agas->vtable;
}
