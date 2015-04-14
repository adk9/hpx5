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
#ifndef LIBHPX_GAS_H
#define LIBHPX_GAS_H

#include <hpx/hpx.h>
#include <libhpx/config.h>
#include <libhpx/system.h>

/// Forward declarations.
/// @{
struct boot;
/// @}

/// Generic object oriented interface to the global address space.
typedef struct gas {
  hpx_gas_t type;

  // Initialization
  void (*delete)(struct gas *gas)
    HPX_NON_NULL(1);

  size_t (*local_size)(struct gas *gas)
    HPX_NON_NULL(1);

  void *(*local_base)(struct gas *gas)
    HPX_NON_NULL(1);

  int64_t (*sub)(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize);
  hpx_addr_t (*add)(hpx_addr_t gva, int64_t bytes, uint32_t bsize);

  // implement hpx/gas.h
  __typeof(HPX_THERE) *there;
  __typeof(hpx_gas_try_pin) *try_pin;
  __typeof(hpx_gas_unpin) *unpin;
  __typeof(hpx_gas_alloc_cyclic) *alloc_cyclic;
  __typeof(hpx_gas_calloc_cyclic) *calloc_cyclic;
  __typeof(hpx_gas_alloc_blocked) *alloc_blocked;
  __typeof(hpx_gas_calloc_blocked) *calloc_blocked;
  __typeof(hpx_gas_alloc_local) *alloc_local;
  __typeof(hpx_gas_calloc_local) *calloc_local;
  __typeof(hpx_gas_free) *free;
  __typeof(hpx_gas_move) *move;
  __typeof(hpx_gas_memget) *memget;
  __typeof(hpx_gas_memput) *memput;
  __typeof(hpx_gas_memcpy) *memcpy;

  // network operation
  uint32_t (*owner_of)(hpx_addr_t gpa);
  // uint64_t (*offset_of)(hpx_addr_t gpa);

  // quick hack for the global allocator
  system_mmap_t mmap;
  system_munmap_t munmap;
} gas_t;

gas_t *gas_new(const config_t *cfg, struct boot *boot)
  HPX_INTERNAL HPX_NON_NULL(1,2);

inline static void gas_delete(gas_t *gas) {
  assert(gas && gas->delete);
  gas->delete(gas);
}

inline static uint32_t gas_owner_of(gas_t *gas, hpx_addr_t addr) {
  assert(gas && gas->owner_of);
  return gas->owner_of(addr);
}

static inline size_t gas_local_size(gas_t *gas) {
  assert(gas && gas->local_size);
  return gas->local_size(gas);
}

inline static void *gas_local_base(gas_t *gas) {
  assert(gas && gas->local_base);
  return gas->local_base(gas);
}

static inline void *gas_mmap(void *obj, void *addr, size_t bytes, size_t align) {
  gas_t *gas = obj;
  return gas->mmap(obj, addr, bytes, align);
}

static inline void gas_munmap(void *obj, void *addr, size_t size) {
  gas_t *gas = obj;
  gas->munmap(obj, addr, size);
}

#endif// LIBHPX_GAS_H
