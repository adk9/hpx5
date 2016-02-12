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

#ifndef LIBHPX_REBALANCING_H
#define LIBHPX_REBALANCING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hpx/hpx.h>

#if defined(HAVE_AGAS) && defined(HAVE_REBALANCING)
    
// AGAS-based Rebalancer API
int libhpx_rebalancer_init(void);
void libhpx_rebalancer_finalize(void);

// Register a worker thread with the rebalancer
void libhpx_rebalancer_bind_worker(void);

// Record the GAS block entry access info in the rebalancer
void libhpx_rebalancer_add_entry(int src, int dst, hpx_addr_t block,
                                 size_t size);

int libhpx_rebalancer_start(hpx_addr_t sync);

#else

#define libhpx_rebalancer_init()
#define libhpx_rebalancer_finalize()
#define libhpx_rebalancer_bind_worker()
#define libhpx_rebalancer_add_entry(...)
#define libhpx_rebalancer_start(...)

#endif

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_REBALANCING_H
