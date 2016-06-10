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

#ifndef LIBHPX_GAS_AFFINITY_H
#define LIBHPX_GAS_AFFINITY_H

#include <hpx/hpx.h>

#ifdef __cplusplus
extern "C" {
#endif

void affinity_init(void *);
void affinity_fini(void *);
void affinity_set(void *, hpx_addr_t gva, int worker);
void affinity_clear(void *, hpx_addr_t gva);
int affinity_get(const void *, hpx_addr_t gva);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_GAS_AFFINITY_H
