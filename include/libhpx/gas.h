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

struct boot_class;
struct transport_class;

typedef struct as_class as_class_t;
struct as_class {
  void *(*malloc)(size_t bytes);
  void (*free)(void *ptr);
  void *(*calloc)(size_t nmemb, size_t size);
  void *(*realloc)(void *ptr, size_t size);
  void *(*valloc)(size_t size);
  void *(*memalign)(size_t boundary, size_t size);
  int (*posix_memalign)(void **memptr, size_t alignment, size_t size);
};

/// Generic object oriented interface to the global address space.
typedef struct gas_class gas_class_t;
struct gas_class {
  hpx_gas_t type;

  // Initialization
  void (*delete)(gas_class_t *gas);
  int (*join)(void);
  void (*leave)(void);
  bool (*is_global)(gas_class_t *gas, void *addr);

  //
  as_class_t global;
  as_class_t local;

  // Dealing with global addresses
  uint32_t (*locality_of)(hpx_addr_t gva);
  uint64_t (*offset_of)(hpx_addr_t gva, uint32_t bsize);
  uint32_t (*phase_of)(hpx_addr_t gva, uint32_t bsize);

  int64_t (*sub)(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize);
  hpx_addr_t (*add)(hpx_addr_t gva, int64_t bytes, uint32_t bsize);

  hpx_addr_t (*lva_to_gva)(void *lva);
  void *(*gva_to_lva)(hpx_addr_t gva);
};

gas_class_t *gas_smp_new(size_t heap_size, struct boot_class *boot,
                         struct transport_class *transport)
  HPX_INTERNAL HPX_NON_NULL(2,3);

gas_class_t *gas_pgas_new(size_t heap_size, struct boot_class *boot,
                         struct transport_class *transport)
  HPX_INTERNAL HPX_NON_NULL(2,3);

gas_class_t *gas_new(size_t heap_size, struct boot_class *boot,
                     struct transport_class *transport, hpx_gas_t type)
  HPX_INTERNAL HPX_NON_NULL(2,3);

inline static void gas_delete(gas_class_t *gas) {
  gas->delete(gas);
}

inline static int gas_join(gas_class_t *gas) {
  return gas->join();
}

inline static void gas_leave(gas_class_t *gas) {
  gas->leave();
}

#endif// LIBHPX_GAS_H
