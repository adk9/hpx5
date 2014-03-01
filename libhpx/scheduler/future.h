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
#ifndef LIBHPX_FUTURE_H
#define LIBHPX_FUTURE_H

/// ----------------------------------------------------------------------------
/// @file future.h
/// Declares the future structure, and its internal interface.
/// ----------------------------------------------------------------------------
#include "attributes.h"
#include "lco.h"

HPX_INTERNAL int future_init_module(void);
HPX_INTERNAL void future_fini_module(void);


/// ----------------------------------------------------------------------------
/// Future structure.
/// ----------------------------------------------------------------------------
typedef struct future future_t;
struct future {
  lco_t lco;                                    // future "is-an" lco
  void *value;
};

#endif // LIBHPX_FUTURE_H
