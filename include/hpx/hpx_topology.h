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
#ifndef HPX_TOPOLOGY_H
#define HPX_TOPOLOGY_H

#include "hpx/hpx_config.h"

/// ----------------------------------------------------------------------------
/// HPX topology interface
/// ----------------------------------------------------------------------------
hpx_locality_t hpx_get_my_rank(void);
int hpx_get_num_ranks(void);
int hpx_get_num_threads(void);
int hpx_get_my_thread_id(void);

#define HPX_LOCALITY_ID hpx_get_my_rank()
#define HPX_LOCALITIES hpx_get_num_ranks()
#define HPX_THREAD_ID hpx_get_my_thread_id()
#define HPX_THREADS hpx_get_num_threads()

#endif
