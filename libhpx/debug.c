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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include "hpx/hpx.h"

#include "libhpx/debug.h"

void
dbg_log1(unsigned line, const char *f, const char *fmt, ...) {
  printf("LIBHPX<%d,%d>: (%s:%u) ", hpx_get_my_rank(),
         hpx_get_my_thread_id(), f, line);

  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  fflush(stdout);
}

int
dbg_error1(unsigned line, const char *f, const char *fmt, ...) {
  fprintf(stderr, "LIBHPX<%d,%d>: (%s:%u) ", hpx_get_my_rank(),
          hpx_get_my_thread_id(), f, line);

  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  fflush(stderr);
  return HPX_ERROR;
}
