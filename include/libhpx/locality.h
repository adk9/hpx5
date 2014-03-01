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
#ifndef LIBHPX_LOCALITY_H
#define LIBHPX_LOCALITY_H

#include "hpx.h"

/// ----------------------------------------------------------------------------
/// @file libhpx/locality.h
/// @brief Provides an abstraction for system details required by the scheduler
///        and network.
///
/// This locality head provides the following abstractions used by the rest of
/// libhpx.
///
///  1) Locality information.
///  2) Topology information.
///  3) Native threads.
///
/// ----------------------------------------------------------------------------

/// Initialize and finalize the locality. This must be performed first.
HPX_INTERNAL int locality_init_module(const hpx_config_t *config);
HPX_INTERNAL void locality_fini_module(void);

/// Register an active message handler. The implementation for this depends on
/// if we have a symmetric code region or not.
HPX_INTERNAL hpx_action_t locality_action_register(const char *id, hpx_action_handler_t f);
HPX_INTERNAL hpx_action_handler_t locality_action_lookup(hpx_action_t);

/// Query the locality for topology information.
HPX_INTERNAL int locality_get_rank(void);
HPX_INTERNAL int locality_get_n_ranks(void);
HPX_INTERNAL int locality_get_n_processors(void);

/// Start and stop native threads.
typedef int native_thread_t;
HPX_INTERNAL native_thread_t locality_thread_self(void);
HPX_INTERNAL native_thread_t locality_thread_new(void);
HPX_INTERNAL int locality_thread_start(native_thread_t thread, void *(*f)(void*), void *args);
HPX_INTERNAL int locality_thread_cancel(native_thread_t thread);

#endif // LIBHPX_LOCALITY_H
