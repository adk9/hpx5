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
#ifndef LIBHPX_EXAMPLES_DEBUG_H
#define LIBHPX_EXAMPLES_DEBUG_H

#include <limits.h>

#define ALL_RANKS INT_MAX
#define NO_RANKS -1

void wait_for_debugger(int rank);

#endif // LIBHPX_EXAMPLES_DEBUG_H
