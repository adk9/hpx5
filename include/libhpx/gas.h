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
#ifndef LIBHPX_NETWORK_GAS_H
#define LIBHPX_NETWORK_GAS_H

#include "hpx/hpx.h"

struct boot;

typedef struct gas gas_t;

struct gas {
  void       (*delete)(gas_t *gas);
  hpx_addr_t (*alloc)(size_t bytes, gas_t *gas);
  hpx_addr_t (*global_alloc)(size_t n, uint32_t bytes, gas_t *gas);
  int        (*where)(const hpx_addr_t addr, gas_t *gas);
  bool       (*try_pin)(const hpx_addr_t addr, void **local, gas_t *gas);
  void       (*unpin)(const hpx_addr_t addr, gas_t *gas);
};

HPX_INTERNAL gas_t *gas_pgas_new(const struct boot *boot);
HPX_INTERNAL gas_t *gas_agas_new(const struct boot *boot);


/// ----------------------------------------------------------------------------
/// ----------------------------------------------------------------------------
inline static void gas_delete(gas_t *gas) {
  gas->delete(gas);
}


/// ----------------------------------------------------------------------------
/// ----------------------------------------------------------------------------
inline static hpx_addr_t gas_alloc(size_t bytes, gas_t *gas) {
  return gas->alloc(bytes,gas);
}


inline static hpx_addr_t gas_global_alloc(size_t n, uint32_t bytes, gas_t *gas)
{
  return gas->global_alloc(n, bytes, gas);
}


/// ----------------------------------------------------------------------------
/// ----------------------------------------------------------------------------
inline static int gas_where(const hpx_addr_t addr, gas_t *gas) {
  return gas->where(addr, gas);
}


inline static bool gas_try_pin(const hpx_addr_t addr, void **local, gas_t *gas)
{
  return gas->try_pin(addr, local, gas);
}


inline static void gas_unpin(const hpx_addr_t addr, gas_t *gas) {
  gas->unpin(addr, gas);
}


#endif // LIBHPX_NETWORK_GAS_H
