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

#include <fcntl.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

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

#define _vprint_console(c,id,fmt,...)               \
  fprintf(stderr, "%d,%d,%"PRIu64",%s,%s" fmt "\n", \
    here->rank, self->id,                           \
    hpx_time_from_start_ns(hpx_time_now()),         \
    HPX_TRACE_CLASS_TO_STRING[ceil_log2_32(c)],     \
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
      _vprint_console(c, id, ",%"PRIu64, va_arg(vargs, uint64_t));
      break;
    case 2:
      _vprint_console(c, id, ",%"PRIu64",%"PRIu64,
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t));
      break;
    case 3:
      _vprint_console(c, id, ",%"PRIu64",%"PRIu64",%"PRIu64,
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t));
      break;
    case 4:
      _vprint_console(c, id, ",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64,
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t));
      break;
    case 5:
      _vprint_console(c, id, ",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64
                      ",%"PRIu64,
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t));
      break;
    case 6:
      _vprint_console(c, id, ",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64
                      ",%"PRIu64",%"PRIu64,
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t),
                      va_arg(vargs, uint64_t));
      break;
    case 7:
    default:
      _vprint_console(c, id, ",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64
                      ",%"PRIu64",%"PRIu64",%"PRIu64,
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
  pthread_mutex_destroy(&here->trace_lock);
}

static void _phase_begin(void) {
  pthread_mutex_lock(&here->trace_lock);
  here->tracer->active = true;
  pthread_mutex_unlock(&here->trace_lock);
}

static void _phase_end(void) {
  pthread_mutex_lock(&here->trace_lock);
  here->tracer->active = false;
  pthread_mutex_unlock(&here->trace_lock);
}

trace_t *trace_console_new(const config_t *cfg) {
  trace_t *trace = malloc(sizeof(*trace));
  dbg_assert(trace);

  trace->type        = HPX_TRACE_BACKEND_CONSOLE;
  trace->start       = _start;
  trace->destroy     = _destroy;
  trace->vappend     = _vappend;
  trace->phase_begin = _phase_begin;
  trace->phase_end   = _phase_end;
  trace->active      = false;
  pthread_mutex_init(&here->trace_lock, NULL);
  return trace;
}
