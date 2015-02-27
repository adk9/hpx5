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
#ifndef HPX_TOPOLOGY_H
#define HPX_TOPOLOGY_H

/// @file include/hpx/topology.h
/// @brief HPX topology interface

/// Get the "rank" the current code is running on
/// @returns the rank at which the current code is executing
int hpx_get_my_rank(void);

/// Get the number of ranks currently available
/// @returns the number of ranks in the system
int hpx_get_num_ranks(void);

/// Get the number of heavy-weight threads at the current locality
/// These threads are the heavy-weight threads used internally by HPX
/// and are not the same as HPX threads
/// @returns the number of heavy-weight threads at the current locality
int hpx_get_num_threads(void);

/// Get the thread id of the current heavy-weight thread
/// These threads are the heavy-weight threads used internally by HPX
/// and are not the same as HPX threads.
/// If you need an id for a light-weight HPX thread use
/// hpx_thread_get_tls_id().
/// @returns an id number representing the current heavy-weight thread
int hpx_get_my_thread_id(void);

/// @copydoc hpx_get_my_rank()
#define HPX_LOCALITY_ID hpx_get_my_rank()

/// @copydoc hpx_get_num_ranks()
#define HPX_LOCALITIES hpx_get_num_ranks()

/// @copydoc hpx_get_num_thread()
#define HPX_THREADS hpx_get_num_threads()

/// @copydoc hpx_get_my_thread_id()
#define HPX_THREAD_ID hpx_get_my_thread_id()

#endif
