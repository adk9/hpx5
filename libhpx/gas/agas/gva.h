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

typedef union {
  hpx_addr_t addr;
  struct {
    uint64_t offset:48;
    uint64_t large:1;
    uint64_t size_class:3;
    uint64_t :12;
  } bits;
} gva_t;


static inline uint32_t gva_home(hpx_addr_t gva) {
  return 0;
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

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_GAS_AGAS_GVA_H
