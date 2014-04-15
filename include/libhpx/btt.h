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
  void (*delete)(btt_class_t *btt);
  bool (*try_pin)(btt_class_t *btt, hpx_addr_t addr, void **out);
  void (*unpin)(btt_class_t *btt, hpx_addr_t addr);
  void (*invalidate)(btt_class_t *btt, hpx_addr_t addr);
  void (*insert)(btt_class_t *btt, hpx_addr_t addr, void *base);
  void (*remap)(btt_class_t *btt, hpx_addr_t src, hpx_addr_t dst, hpx_addr_t lco);

  uint32_t (*owner)(btt_class_t *btt, hpx_addr_t addr);
  uint32_t (*home)(btt_class_t *btt, hpx_addr_t addr);
};


HPX_INTERNAL btt_class_t *btt_pgas_new(void);
HPX_INTERNAL btt_class_t *btt_agas_new(void);
HPX_INTERNAL btt_class_t *btt_new(hpx_gas_t type);


/// Convenience interface.
inline static void btt_delete(btt_class_t *btt) {
  btt->delete(btt);
}


inline static bool btt_try_pin(btt_class_t *btt, hpx_addr_t addr, void **out) {
  return btt->try_pin(btt, addr, out);
}


inline static void btt_unpin(btt_class_t *btt, hpx_addr_t addr) {
  btt->unpin(btt, addr);
}


inline static void btt_invalidate(btt_class_t *btt, hpx_addr_t addr) {
  btt->invalidate(btt, addr);
}


inline static void btt_insert(btt_class_t *btt, hpx_addr_t addr, void *base) {
  btt->insert(btt, addr, base);
}

inline static void btt_remap(btt_class_t *btt, hpx_addr_t src, hpx_addr_t dst, hpx_addr_t lco) {
  btt->remap(btt, src, dst, lco);
}

inline static uint32_t btt_owner(btt_class_t *btt, hpx_addr_t addr) {
  return btt->owner(btt, addr);
}


inline static uint32_t btt_home(btt_class_t *btt, hpx_addr_t addr) {
  return btt->home(btt, addr);
}


#endif // LIBHPX_BTT_H
