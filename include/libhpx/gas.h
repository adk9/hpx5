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
#ifndef LIBHPX_GAS_H
#define LIBHPX_GAS_H

#include <hpx/hpx.h>
#include "libhpx/config.h"

/// Forward declarations.
/// @{
struct boot;
struct transport;
/// @}

/// Generic object oriented interface to the global address space.
typedef struct gas {
  hpx_gas_t type;

  // Initialization
  void (*delete)(struct gas *gas)
    HPX_NON_NULL(1);

  int (*join)(void);
  void (*leave)(void);

  bool (*is_global)(struct gas *gas, void *addr)
    HPX_NON_NULL(1);

  size_t (*local_size)(struct gas *gas)
    HPX_NON_NULL(1);

  void *(*local_base)(struct gas *gas)
    HPX_NON_NULL(1);

  // Dealing with global addresses
  uint32_t (*locality_of)(hpx_addr_t gva);

  int64_t (*sub)(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize);
  hpx_addr_t (*add)(hpx_addr_t gva, int64_t bytes, uint32_t bsize);

  hpx_addr_t (*lva_to_gva)(void *lva);
  void *(*gva_to_lva)(hpx_addr_t gva);

  // implement hpx/gas.h
  __typeof(HPX_THERE) *there;
  __typeof(hpx_gas_try_pin) *try_pin;
  __typeof(hpx_gas_unpin) *unpin;
  __typeof(hpx_gas_global_alloc) *cyclic_alloc;
  __typeof(hpx_gas_global_calloc) *cyclic_calloc;
  __typeof(hpx_gas_alloc) *local_alloc;
  __typeof(hpx_gas_free) *free;
  __typeof(hpx_gas_move) *move;
  __typeof(hpx_gas_memget) *memget;
  __typeof(hpx_gas_memput) *memput;
  __typeof(hpx_gas_memcpy) *memcpy;

  // network operation
  uint32_t (*owner_of)(hpx_addr_t gpa);
  uint64_t (*offset_of)(hpx_addr_t gpa);
} gas_t;

gas_t *gas_smp_new(void)
  HPX_INTERNAL;

gas_t *gas_pgas_new(size_t heap_size, struct boot *, struct transport *)
  HPX_INTERNAL HPX_NON_NULL(2,3);

gas_t *gas_new(size_t heap_size, struct boot *, struct transport *, hpx_gas_t type)
  HPX_INTERNAL HPX_NON_NULL(2,3);

inline static void gas_delete(gas_t *gas) {
  assert(gas && gas->delete);
  gas->delete(gas);
}

inline static int gas_join(gas_t *gas) {
  assert(gas && gas->join);
  return gas->join();
}

inline static void gas_leave(gas_t *gas) {
  assert(gas && gas->leave);
  gas->leave();
}

inline static uint32_t gas_owner_of(gas_t *gas, hpx_addr_t addr) {
  assert(gas && gas->owner_of);
  return gas->owner_of(addr);
}

inline static uint64_t gas_offset_of(gas_t *gas, hpx_addr_t gpa) {
  assert(gas && gas->offset_of);
  return gas->offset_of(gpa);
}

#endif// LIBHPX_GAS_H
