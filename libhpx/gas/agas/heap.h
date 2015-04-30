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
#ifndef LIBHPX_GAS_AGAS_HEAP_H
#define LIBHPX_GAS_AGAS_HEAP_H

#ifdef __cplusplus
extern "C" {
#else
# include <stdbool.h>
#endif

#include <hpx/attributes.h>

/// The heap manages the local heap allocation.

HPX_INTERNAL void *heap_lva_to_chunk(void *heap, void *lva);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_GAS_AGAS_HEAP_H
