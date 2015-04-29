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

#include <hpx/hpx.h>

static inline uint32_t gva_home(hpx_addr_t gva) {
  return 0;
}

static inline uint64_t gva_to_key(hpx_addr_t gva) {
  return gva;
}

#endif // LIBHPX_GAS_AGAS_GVA_H
