/*
  ====================================================================
  High Performance ParalleX Library (libhpx)

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/
#ifndef LIBSYNC_BACKOFF_H
#define LIBSYNC_BACKOFF_H

#include "hpx/attributes.h"

void backoff(int *prev) HPX_NOINLINE;

#endif // LIBSYNC_BACKOFF_H
