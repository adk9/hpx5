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

/// The default affinity infrastructure uses a stack hashtable.
int affinity_of(const void *, hpx_addr_t gva);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_GAS_AFFINITY_H
