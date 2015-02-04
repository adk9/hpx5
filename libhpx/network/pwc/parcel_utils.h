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
#ifndef LIBHPX_NETWORK_PWC_PARCEL_UTILS_H
#define LIBHPX_NETWORK_PWC_PARCEL_UTILS_H

#include "libhpx/parcel.h"

static inline uint32_t pwc_prefix_size(void) {
  return offsetof(hpx_parcel_t, size);
}

static inline uint32_t pwc_network_size(const hpx_parcel_t *p) {
  return parcel_size(p) - pwc_prefix_size();
}

static inline void *pwc_network_offset(hpx_parcel_t *p) {
  return &p->size;
}

#endif // LIBHPX_NETWORK_ISIR_PARCEL_UTILS_H
