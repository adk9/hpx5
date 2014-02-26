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

HPX_INTERNAL int locality_init_module(int threads);
HPX_INTERNAL void locality_fini_module(void);
HPX_INTERNAL void locality_register_thread_callbacks(int (*init)(void), void(*fini)(void));


#endif // LIBHPX_LOCALITY_H
