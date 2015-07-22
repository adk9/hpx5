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
#ifdef HAVE_PAPI
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <hpx/hpx.h>
#include <libsync/sync.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/profiling.h>

/// Each locality maintains a single profile log
static profile_t _profile_log = PROFILE_INIT;

int prof_init(struct config *cfg){
#ifndef ENABLE_INSTRUMENTATION
  return LIBHPX_OK;
#endif

  PAPI_library_init(PAPI_VER_CURRENT);
  int max_counters = PAPI_num_counters();
  return LIBHPX_OK;
}

#endif
