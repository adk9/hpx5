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
#ifndef LIBHPX_BTT_H
#define LIBHPX_BTT_H

#include "hpx/hpx.h"

typedef struct btt_class btt_class_t;
struct btt_class {
  hpx_gas_t type;
  void (*delete)(btt_class_t *btt);
  bool (*try_pin)(btt_class_t *btt, hpx_addr_t addr, void **out);
  void (*unpin)(btt_class_t *btt, hpx_addr_t addr);


  /// --------------------------------------------------------------------------
  /// Inserts a mapping in the BTT.
  /// --------------------------------------------------------------------------
  void (*insert)(btt_class_t *btt, hpx_addr_t addr, void *base);


  /// --------------------------------------------------------------------------
  /// Update a mapping to be a forward.
  ///
  ///   if (invalid) abort
  ///   if (rank == here->rank) return false
  ///   if (out) *out = map(addr)
  ///   map(addr) = rank
  ///   return local
  /// --------------------------------------------------------------------------
  bool (*forward)(btt_class_t *btt, hpx_addr_t addr, uint32_t rank, void **out);

  uint32_t (*owner)(btt_class_t *btt, hpx_addr_t addr);

  uint32_t (*home)(btt_class_t *btt, hpx_addr_t addr);
};


HPX_INTERNAL btt_class_t *btt_local_only_new(size_t heap_size);
HPX_INTERNAL btt_class_t *btt_pgas_new(size_t heap_size);
HPX_INTERNAL btt_class_t *btt_agas_new(size_t heap_size);
HPX_INTERNAL btt_class_t *btt_agas_switch_new(size_t heap_size);
HPX_INTERNAL btt_class_t *btt_new(hpx_gas_t type, size_t heap_size);


/// Convenience interface.
inline static void btt_delete(btt_class_t *btt) {
  btt->delete(btt);
}

inline static hpx_gas_t btt_type(btt_class_t *btt) {
  return btt->type;
}

inline static bool btt_try_pin(btt_class_t *btt, hpx_addr_t addr, void **out) {
  return btt->try_pin(btt, addr, out);
}


inline static void btt_unpin(btt_class_t *btt, hpx_addr_t addr) {
  btt->unpin(btt, addr);
}


inline static void btt_insert(btt_class_t *btt, hpx_addr_t addr, void *base) {
  btt->insert(btt, addr, base);
}


inline static bool btt_forward(btt_class_t *btt, hpx_addr_t addr, uint32_t rank,
                               void **out) {
  return btt->forward(btt, addr, rank, out);
}


inline static uint32_t btt_owner(btt_class_t *btt, hpx_addr_t addr) {
  return btt->owner(btt, addr);
}


inline static uint32_t btt_home(btt_class_t *btt, hpx_addr_t addr) {
  return btt->home(btt, addr);
}


#endif // LIBHPX_BTT_H
