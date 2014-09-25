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
#ifndef LIBHPX_GAS_PGAS_H
#define LIBHPX_GAS_PGAS_H

/// @file libhpx/gas/pgas.h
/// @brief PGAS specific interface to the global address space.
///
/// In HPX, allocations are done in terms of "blocks," which are user-sized,
/// byte-addressible buffers.
///
/// In the PGAS model, global address values uniquely identify blocks that
/// remain fixed at the locality at which they were originally allocated.
///
/// We distinguish two forms of allocation, cyclic and acyclic. Cyclic
/// allocations are arrays of blocks that have contiguous global addresses but
/// are distributed round-robin across localities. Cyclic allocation regions
/// always start with the root node. Acyclic allocations are arrays of blocks
/// that have contiguous global addresses but are allocated on only one node.
///

#include <stddef.h>

int lhpx_pgas_init(size_t heap_size);
int lhpx_pgas_init_worker();
void lphx_pgas_fini_worker();
void lhpx_pgas_fini(void);

#endif
