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
#ifndef LIBHPX_GAS_AGAS_GVA_H
#define LIBHPX_GAS_AGAS_GVA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hpx/hpx.h>

/// Set up some limitations for the AGAS implementation for now.
#define   GVA_RANK_BITS 12
#define GVA_ACTION_BITS 12
#define  GVA_CLASS_BITS 3
#define GVA_OFFSET_BITS (48 - GVA_RANK_BITS)

typedef union {
  hpx_addr_t addr;
  struct {
    uint64_t     offset :GVA_OFFSET_BITS;
    uint64_t       home :GVA_RANK_BITS;
    uint64_t      large :1;
    uint64_t size_class :GVA_CLASS_BITS;
    uint64_t            :GVA_ACTION_BITS;
  } bits;
} gva_t;

_HPX_ASSERT(sizeof(gva_t) == sizeof(hpx_addr_t), gva_bitfield_packing_failed);

static inline uint32_t gva_home(hpx_addr_t gva) {
  gva_t addr = {
    .addr = gva
  };
  return addr.bits.home;
}

static inline int gva_to_size_class(hpx_addr_t gva) {
  gva_t addr = {
    .addr = gva
  };
  return  addr.bits.size_class;
}

static inline uint64_t gva_to_key(hpx_addr_t gva) {
  return gva;
}

static inline uint64_t lva_to_gva_offset(void *lva) {
  return 0;
}

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_GAS_AGAS_GVA_H
