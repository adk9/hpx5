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

/// ----------------------------------------------------------------------------
/// Agas
/// ----------------------------------------------------------------------------
#include "libhpx/gas.h"
#include "addr.h"

static void **_btt = NULL;                      // the block translation table


static void _delete(gas_t *gas) {
}

static hpx_addr_t _alloc(size_t bytes, gas_t *gas) {
  return HPX_NULL;
}

static hpx_addr_t _global_alloc(size_t n, uint32_t butes, gas_t *gas) {
  return HPX_NULL;
}

static int _where(const hpx_addr_t addr, gas_t *gas) {
  return addr_block_id(addr);
}

static bool _try_pin(const hpx_addr_t addr, gas_t *gas) {
  return false;
}

static void _unpin(const hpx_addr_t addr, gas_t *gas) {
}

static gas_t agas = {
  .delete       = _delete,
  .alloc        = _alloc,
  .global_alloc = _global_alloc,
  .where        = _where,
  .try_pin      = _try_pin,
  .unpin        = _unpin
};

gas_t *gas_agas_new(const struct boot *boot) {
  return &agas;
}
