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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>

#include <hpx/hpx.h>
#include <libsync/sync.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/instrumentation.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include "logtable.h"

/// We're keeping one log per event per locality. Here are their headers.
static logtable_t _logs[HPX_INST_NUM_EVENTS] = {LOGTABLE_INIT};

static void _log_create(int class, int id, size_t size, hpx_time_t now) {
  char filename[256];
  snprintf(filename, 256, "event.%d.%d.%d.%s.%s.log",
           class, id, hpx_get_my_rank(),
           INST_CLASS_TO_STRING[class],
           INST_EVENT_TO_STRING[id]);

  int e = logtable_init(&_logs[id], filename, size, class, id, now);
  if (e) {
    log_error("failed to initialize log file %s\n", filename);
  }
}

static int _chdir(const char *dir) {
  // change to user-specified root directory
  if (0 != chdir(dir)) {
    log_error("Specified root directory for instrumentation not found.");
    return HPX_SUCCESS;
  }

  // create directory name
  time_t t = time(NULL);
  struct tm lt;
  localtime_r(&t, &lt);
  char dirname[256];
  snprintf(dirname, 256, "hpx.%.4d%.2d%.2d.%.2d%.2d",
           lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min);

  // try and create the directory---we don't care if it's already there
  int e = mkdir(dirname, 0777);
  if (e) {
    if (errno != EEXIST) {
      return log_error("Could not create %s for instrumentation\n", dirname);
    }
  }
  e = chdir(dirname);
  if (e) {
    return log_error("could not change directories to %s\n", dirname);
  }

  log("initialized %s/%s for tracing\n", dir, dirname);
  return LIBHPX_OK;
}

int inst_init(config_t *cfg) {
#ifndef ENABLE_INSTRUMENTATION
  return LIBHPX_OK;
#endif
  if (!config_trace_classes_isset(cfg, LIBHPX_OPT_BITSET_ALL)) {
    return LIBHPX_OK;
  }

  if (!config_trace_at_isset(cfg, hpx_get_my_rank())) {
    return LIBHPX_OK;
  }

  if (_chdir(cfg->trace_dir)) {
    return LIBHPX_OK;
  }

  // create log files
  hpx_time_t start = hpx_time_now();
  for (int cl = 0, e = HPX_INST_NUM_CLASSES; cl < e; ++cl) {
    for (int id = INST_OFFSETS[cl], e = INST_OFFSETS[cl + 1]; id < e; ++id) {
      if (inst_trace_class(cl)) {
        _log_create(cl, id, cfg->trace_filesize, start);
      }
    }
  }

  return LIBHPX_OK;
}

void inst_fini(void) {
  for (int i = 0, e = HPX_INST_NUM_EVENTS; i < e; ++i) {
    logtable_fini(&_logs[i]);
  }
}

void inst_vtrace(int UNUNSED, int n, int id, ...) {
  dbg_assert_str(n < 5, "can only trace up to 4 user values\n");
  logtable_t *log = &_logs[id];
  if (!log->records) {
    return;
  }

  uint64_t args[4];
  va_list vargs;
  va_start(vargs, id);
  for (int i = 0; i < n; ++i) {
    args[i] = va_arg(vargs, uint64_t);
  }
  va_end(vargs);
  for (int i = n; i < 4; ++i) {
    args[i] = 0;
  }
  logtable_append(log, args[0], args[1], args[2], args[3]);
}
