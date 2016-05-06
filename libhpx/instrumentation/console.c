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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pwd.h>

#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/scheduler.h>
#include <hpx/hpx.h>
#include "metadata.h"

/// This will output a list of action ids and names.
static void _print_actions(void) {
  if (here->rank == 0) {
    for (int i = 1, e = action_table_size(); i < e; ++i) {
      const char *tag = action_is_internal(i) ? "INTERNAL" : "USER";
      fprintf(stderr, "%d,%s,%s\n", i, actions[i].key, tag);
    }
  }
}

#define _vprint_console(c,id,fmt,...)           \
  fprintf(stderr, "%d,%d,%lu,%s,%s" fmt "\n",   \
    here->rank, self->id,                       \
    hpx_time_from_start_ns(hpx_time_now()),     \
    HPX_TRACE_CLASS_TO_STRING[c],               \
    TRACE_EVENT_TO_STRING[id], ##__VA_ARGS__)

static void _vappend(int UNUSED, int n, int id, ...) {
  if (!self) {
    return;
  }

  int c = TRACE_EVENT_TO_CLASS[id];
  if (!inst_trace_class(c)) {
    return;
  }

  va_list vargs;
  va_start(vargs, id);

  switch (n) {
    case 0:
      _vprint_console(c, id, "");
      break;
    case 1:
      _vprint_console(c, id, ",%lu", va_arg(vargs, uint64_t));
      break;
    case 2:
      _vprint_console(c, id, ",%lu,%lu",
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t));
      break;
    case 3:
      _vprint_console(c, id, ",%lu,%lu,%lu",
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t));
      break;
    case 4:
      _vprint_console(c, id, ",%lu,%lu,%lu,%lu",
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t));
      break;
    case 5:
      _vprint_console(c, id, ",%lu,%lu,%lu,%lu,%lu",
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t));
      break;
    case 6:
      _vprint_console(c, id, ",%lu,%lu,%lu,%lu,%lu,%lu",
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t));
      break;
    case 7:
    default:
      _vprint_console(c, id, ",%lu,%lu,%lu,%lu,%lu,%lu,%lu",
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t));
      break;
  }

  va_end(vargs);
}

static void _start(void) {
  if (inst_trace_class(HPX_TRACE_PARCEL)) {
    _print_actions();
  }
}

static void _destroy(void) {
}

trace_t *trace_console_new(const config_t *cfg) {
  trace_t *trace = malloc(sizeof(*trace));
  dbg_assert(trace);

  trace->type    = HPX_TRACE_BACKEND_CONSOLE;
  trace->start   = _start;
  trace->destroy = _destroy;
  trace->vappend = _vappend;
  return trace;
}
