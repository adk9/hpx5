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

  void (*bind)(gas_class_t *gas, struct transport_class *transport);
  void (*delete)(gas_class_t *gas);
  int (*join)(void);
  void (*leave)(void);
  bool (*is_global)(gas_class_t *gas, void *addr);

  as_class_t global;
  as_class_t local;
};

gas_class_t *gas_smp_new(size_t heap_size) HPX_INTERNAL;
gas_class_t *gas_pgas_new(size_t heap_size) HPX_INTERNAL;
gas_class_t *gas_agas_new(size_t heap_size) HPX_INTERNAL;
gas_class_t *gas_agas_switch_new(size_t heap_size) HPX_INTERNAL;
gas_class_t *gas_new(hpx_gas_t type, size_t heap_size) HPX_INTERNAL;

inline static void gas_bind(gas_class_t *gas, struct transport_class *transport)
{
  gas->bind(gas, transport);
}

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
