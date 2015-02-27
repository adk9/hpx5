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
#ifndef LIBHPX_LIBHPX_H
#define LIBHPX_LIBHPX_H

#include <errno.h>

enum {
  LIBHPX_ENOMEM = -(ENOMEM),
  LIBHPX_EINVAL = -(EINVAL),
  LIBHPX_ERROR = -2,
  LIBHPX_EUNIMPLEMENTED = -1,
  LIBHPX_OK = 0,
  LIBHPX_RETRY
};

#endif  // LIBHPX_LIBHPX_H
