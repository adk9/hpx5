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
#ifndef LIBHPX_TIME_H
#define LIBHPX_TIME_H

#include "attributes.h"

HPX_INTERNAL int time_init_module(void);
HPX_INTERNAL void time_fini_module(void);
HPX_INTERNAL int time_init_thread(void);
HPX_INTERNAL void time_fini_thread(void);

#endif // LIBHPX_TIME_H
