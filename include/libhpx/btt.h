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
  /// Invalidate a mapping.
  ///
  /// Invalidate has slightly complicated semantics. It invalidates a mapping
  /// for the @p addr regardless of the current state for the mapping. If the
  /// mapping was valid, and local, it will return the old mapped value. If the
  /// mapping was invalid, it will return NULL. If the mapping was valid, but it
  /// was a forward to another rank (i.e., a cached mapping), then it will
  /// return NULL.
  /// --------------------------------------------------------------------------
  void *(*invalidate)(btt_class_t *btt, hpx_addr_t addr);

  /// --------------------------------------------------------------------------
  /// Update a mapping to be a forward.
  ///
  /// Returns the old local mapping, if there was one. If there was an old
  /// forwarding rank, or the mapping was invalid, we return NULL.
  /// --------------------------------------------------------------------------
  void *(*update)(btt_class_t *btt, hpx_addr_t addr, uint32_t rank);

  void (*insert)(btt_class_t *btt, hpx_addr_t addr, void *base);

  uint32_t (*owner)(btt_class_t *btt, hpx_addr_t addr);
  uint32_t (*home)(btt_class_t *btt, hpx_addr_t addr);
};


HPX_INTERNAL btt_class_t *btt_pgas_new(void);
HPX_INTERNAL btt_class_t *btt_agas_new(void);
HPX_INTERNAL btt_class_t *btt_agas_switch_new(void);
HPX_INTERNAL btt_class_t *btt_new(hpx_gas_t type);


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


inline static void *btt_invalidate(btt_class_t *btt, hpx_addr_t addr) {
  return btt->invalidate(btt, addr);
}


inline static void btt_insert(btt_class_t *btt, hpx_addr_t addr, void *base) {
  btt->insert(btt, addr, base);
}


inline static uint32_t btt_owner(btt_class_t *btt, hpx_addr_t addr) {
  return btt->owner(btt, addr);
}


inline static uint32_t btt_home(btt_class_t *btt, hpx_addr_t addr) {
  return btt->home(btt, addr);
}


#endif // LIBHPX_BTT_H
