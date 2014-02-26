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

/// ----------------------------------------------------------------------------
/// @file locality.h
///
/// This is libhpx's interface to the hardware locality, providing hardware
/// thread and topology information.
/// ----------------------------------------------------------------------------
#include "attributes.h"

/// ----------------------------------------------------------------------------
/// Initialize the locality.
///
/// @param threads - the number of native threads to spawn.
/// ----------------------------------------------------------------------------
HPX_INTERNAL int locality_init_module(int threads);

/// ----------------------------------------------------------------------------
/// Clean up the locality.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void locality_fini_module(void);

/// ----------------------------------------------------------------------------
/// Register initializers and finalizers for native threads.
///
/// Initializers will be executed in FIFO order, and finalizers will be executed
/// in LIFO order. If an initializer fails to run (returns non-0), then
/// finalizers for initializers that were successful will be run before the
/// thread exits.
///
/// Initializers and finalizers must be registered before the locality is
/// initialized using locality_init_module().
///
/// @param init - an initialization callback
/// @param fini - a finalization callback
/// ----------------------------------------------------------------------------
HPX_INTERNAL void locality_register_thread_callbacks(int (*init)(void),
                                                     void (*fini)(void));

#endif // LIBHPX_LOCALITY_H
