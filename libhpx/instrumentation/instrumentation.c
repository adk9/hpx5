// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <inttypes.h>
#include <limits.h>
#include <unistd.h>
#include <pwd.h>

#include <hpx/hpx.h>
#include <hpx/attributes.h>
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/events.h>
#include <libhpx/instrumentation.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include "file.h"

trace_t *trace_new(const config_t *cfg) {
#ifndef ENABLE_INSTRUMENTATION
  return NULL;
#endif

  // If we're not tracing at this locality, then don't do anything.
  if (!config_trace_at_isset(cfg, HPX_LOCALITY_ID) || !cfg->trace_classes) {
    return NULL;
  }

  libhpx_trace_backend_t type = cfg->trace_backend;
  if (type == HPX_TRACE_BACKEND_DEFAULT || type == HPX_TRACE_BACKEND_FILE) {
    return trace_file_new(cfg);
  } else if (type == HPX_TRACE_BACKEND_CONSOLE) {
    return trace_console_new(cfg);
  } else if (type == HPX_TRACE_BACKEND_STATS) {
    return trace_stats_new(cfg);
  } else {
    dbg_error("unknown trace backend type.\n");
  }
  return NULL;
}

void libhpx_inst_phase_begin() {
  if (here->tracer != NULL) {
    here->tracer->active = true;
  }
}

void libhpx_inst_phase_end() {
  if (here->tracer != NULL) {
    here->tracer->active = false;
  }
}

bool libhpx_inst_tracer_active() {
  if (here->tracer != NULL) {
    return here->tracer->active;
  }
  return false;
}
