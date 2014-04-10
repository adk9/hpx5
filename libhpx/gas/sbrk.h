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
#ifndef LIBHPX_GAS_SBRK_H
#define LIBHPX_GAS_SBRK_H

/// ----------------------------------------------------------------------------
/// ----------------------------------------------------------------------------
#include "hpx/attributes.h"

HPX_INTERNAL void gas_sbrk_init(uint32_t ranks);
HPX_INTERNAL uint32_t gas_sbrk(size_t n);

#endif // LIBHPX_GAS_SBRK_H
