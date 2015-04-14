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

static void _agas_delete(gas_t *gas) {
  free(gas);
}

static int64_t _agas_sub(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  return INT64_MAX;;
}

static hpx_addr_t _agas_add(hpx_addr_t gva, int64_t bytes, uint32_t bsize) {
  return HPX_NULL;
}

static hpx_addr_t _agas_lva_to_gva(const void *lva) {
  return HPX_NULL;
}

static gas_t _agas_vtable = {
  .type           = HPX_GAS_AGAS,
  .delete         = _agas_delete,
  .local_size     = NULL,
  .local_base     = NULL,
  .sub            = _agas_sub,
  .add            = _agas_add,
  .lva_to_gva     = _agas_lva_to_gva,
  .there          = NULL,
  .try_pin        = NULL,
  .unpin          = NULL,
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
  gas_t *gas = malloc(sizeof(*gas));
  *gas = _agas_vtable;
  return gas;
}
