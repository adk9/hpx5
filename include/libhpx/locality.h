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
HPX_INTERNAL int locality_startup(const hpx_config_t *config);
HPX_INTERNAL void locality_shutdown(void);

/// Register an active message handler. The implementation for this depends on
/// if we have a symmetric code region or not. These may be called in elf
/// constructors. No new actions may be registered once hpx_run() has been
/// called.
HPX_INTERNAL hpx_action_t locality_action_register(const char *id, hpx_action_handler_t f);
HPX_INTERNAL hpx_action_handler_t locality_action_lookup(hpx_action_t);

/// Query the locality for topology information.
HPX_INTERNAL int locality_get_rank(void);
HPX_INTERNAL int locality_get_n_ranks(void);
HPX_INTERNAL int locality_get_n_processors(void);

/// Some output wrappers
HPX_INTERNAL void locality_logf1(const char *f, const char *fmt, ...) HPX_PRINTF(2, 3);
HPX_INTERNAL void locality_printe1(const char *f, const char *fmt, ...) HPX_PRINTF(2, 3);

#define locality_logf(...) locality_logf1(__func__, __VA_ARGS__)
#define locality_printe(...) locality_printe1(__func__, __VA_ARGS__)

#endif // LIBHPX_LOCALITY_H
