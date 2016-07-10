// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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
#include <libhpx/string.h>
#include <libhpx/system.h>

#ifdef __cplusplus
namespace libhpx {
namespace gas {
class Affinity {
 public:
  virtual ~Affinity();

  virtual void set(hpx_addr_t gva, int worker) = 0;
  virtual void clear(hpx_addr_t gva) = 0;
  virtual int get(hpx_addr_t gva) const = 0;
};
}
}
extern "C" {
#endif

void* affinity_new(const config_t *config);
void affinity_delete(void *obj);
void affinity_set(void *obj, hpx_addr_t gva, int worker);
void affinity_clear(void *obj, hpx_addr_t gva);
int affinity_get(const void *obj, hpx_addr_t gva);

/// Forward declarations.
/// @{
struct boot;
/// @}

/// Generic object oriented interface to the global address space.
typedef struct gas {
  libhpx_gas_t type;
  class_string_t string;
  uint64_t max_block_size;

  void (*dealloc)(void *gas);
  size_t (*local_size)(const void *gas);
  void *(*local_base)(const void *gas);

  hpx_gas_ptrdiff_t (*sub)(const void *gas, hpx_addr_t lhs, hpx_addr_t rhs,
                 size_t bsize);
  hpx_addr_t (*add)(const void *gas, hpx_addr_t gva, hpx_gas_ptrdiff_t bytes,
                    size_t bsize);

  hpx_addr_t (*there)(const void *gas, uint32_t i);
  uint32_t (*owner_of)(const void *gas, hpx_addr_t gva);
  bool (*try_pin)(void *gas, hpx_addr_t gva, void **local);
  void (*unpin)(void *gas, hpx_addr_t gva);
  void (*free)(void *gas, hpx_addr_t gca, hpx_addr_t rsync);
  void (*set_attr)(void *gas, hpx_addr_t gva, uint32_t attr);

  void (*move)(void *gas, hpx_addr_t src, hpx_addr_t dst, hpx_addr_t lco);

  // implement hpx/gas.h
  __typeof(hpx_gas_alloc_local_attr) *alloc_local;
  __typeof(hpx_gas_calloc_local_attr) *calloc_local;
  __typeof(hpx_gas_alloc_cyclic_attr) *alloc_cyclic;
  __typeof(hpx_gas_calloc_cyclic_attr) *calloc_cyclic;
  __typeof(hpx_gas_alloc_blocked_attr) *alloc_blocked;
  __typeof(hpx_gas_calloc_blocked_attr) *calloc_blocked;
  __typeof(hpx_gas_alloc_user_attr) *alloc_user;
  __typeof(hpx_gas_calloc_user_attr) *calloc_user;

  void *affinity;
} gas_t;

gas_t *gas_new(config_t *cfg, struct boot *boot)
  HPX_MALLOC HPX_NON_NULL(1,2);

inline static void gas_dealloc(void *obj) {
  gas_t *gas = (gas_t*)obj;
  gas->dealloc(gas);
}

inline static uint32_t gas_owner_of(const void *obj, hpx_addr_t gva) {
  const gas_t *gas = (const gas_t*)obj;
  return gas->owner_of(gas, gva);
}

inline static int gas_get_affinity(const void *obj, hpx_addr_t gva) {
  const gas_t *gas = (const gas_t*)obj;
#ifdef __cplusplus
  return static_cast<const libhpx::gas::Affinity*>(gas->affinity)->get(gva);
#else
  return affinity_get(gas->affinity, gva);
#endif
}

inline static void gas_set_affinity(void *obj, hpx_addr_t gva, int worker) {
  gas_t *gas = (gas_t*)obj;
#ifdef __cplusplus
  static_cast<libhpx::gas::Affinity*>(gas->affinity)->set(gva, worker);
#else
  affinity_set(gas->affinity, gva, worker);
#endif
}

inline static void gas_clear_affinity(void *obj, hpx_addr_t gva) {
  gas_t *gas = (gas_t*)obj;
#ifdef __cplusplus
  static_cast<libhpx::gas::Affinity*>(gas->affinity)->clear(gva);
#else
  affinity_clear(gas->affinity, gva);
#endif
}

static inline size_t gas_local_size(void *obj) {
  gas_t *gas = (gas_t*)obj;
  return gas->local_size(gas);
}

inline static void *gas_local_base(void *obj) {
  gas_t *gas = (gas_t*)obj;
  return gas->local_base(gas);
}

static const char* const HPX_GAS_ATTR_TO_STRING[] = {
  "NONE",
  "READONLY",
  "LOAD-BALANCE",
  "LCO"
};


#ifdef __cplusplus
}
#endif

#endif // LIBHPX_GAS_H
