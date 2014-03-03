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
#ifndef LIBHPX_DEBUG_H
#define LIBHPX_DEBUG_H

#include "stdlib.h"
#include "stdio.h"

#define UNIMPLEMENTED() abort()

#define logf(...)                               \
  do {                                          \
    printf("%s() ", __func__);                  \
    printf(__VA_ARGS__);                        \
    fflush(stdout);                             \
  } while (0)

#define printe(...)                             \
  do {                                          \
    fprintf(stderr, "%s() ", __func__);         \
    fprintf(stderr,  __VA_ARGS__);              \
    fflush(stderr);                             \
  } while (0)

#endif // LIBHPX_DEBUG_H
