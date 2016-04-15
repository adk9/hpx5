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
#include "logtable.h"

#ifndef HOST_NAME_MAX
# define HOST_NAME_MAX 255
#endif

/// The path to the log directory.
///
/// This path is set up during inst_init either based on the --hpx-inst-dir
/// runtime option, or using a temporary directory if no dir is specified. It is
/// not initialized if we are not logging instrumentation at this locality, so
/// the special value of NULL is meaningful.
///
/// @{
static const char *_log_path;
/// @}

/// Concatenate two paths.
///
/// This will return a dynamically allocated, '\0' terminated string that joins
/// the two paths with a '/'. The client must free the returned string to
/// prevent memory leaks.
///
/// @param          lhs The left-hand-side of the path.
/// @param          rhs The right-hand-side of the path.
///
/// @return             A dynamically allocated string that is the concatenated
///                     path: "lhs + / + rhs", must be freed by the caller.
static char *_concat_path(const char *lhs, const char *rhs) {
  // length is +1 for '/' and +1 for \00
  int bytes = strlen(lhs) + strlen(rhs) + 2;
  char *out = malloc(bytes);
  snprintf(out, bytes, "%s/%s", lhs, rhs);
  return out;
}

/// Open an output log file of the given name in the log directory.
///
/// This will fopen a file relative to the _log_path, as long as the _log_path
/// is non-NULL. If the _log_path is NULL this is interpreted as disabling
/// instrumentation at this rank and we just return NULL. If the fopen fails
/// this will log an error and return NULL.
///
/// @param          name The name of the file to open.
/// @param             e An error string to print if the fopen fails.
///
/// @return              The opened file, or NULL if no file was opened.
static HPX_NON_NULL(1, 2) FILE *_fopen_log(const char *name, const char *e) {
  if (!_log_path) {
    return NULL;
  }

  char *path = _concat_path(_log_path, name);
  FILE *file = fopen(path, "w");
  if (!file) {
    log_error("failed to open file %s\n %s\n", path, e);
  }
  free(path);
  return file;
}

/// Dump a file that will map this locality ID to a host hame.
static void _dump_hostnames(void) {
  char hostname[HOST_NAME_MAX];
  gethostname(hostname, HOST_NAME_MAX);

  char filename[256];
  snprintf(filename, 256, "hostname.%d", HPX_LOCALITY_ID);

  FILE *file = _fopen_log(filename, "failed to open hostname file");
  if (!file) {
    return;
  }

  fprintf(file, "%s\n", hostname);
  fclose(file);
}

/// This will output a list of action ids and names as a two-column csv file
/// This is so that traced parcels can be interpreted more easily.
static void _dump_actions(void) {
  char filename[256];
  snprintf(filename, 256, "actions.%d.csv", hpx_get_my_rank());

  FILE *file = _fopen_log(filename, "failed to open action file");
  if (!file) {
    return;
  }

  for (int i = 1, e = action_table_size(); i < e; ++i) {
    const char *tag = action_is_internal(i) ? "INTERNAL" : "USER";
    fprintf(file, "%d,%s,%s\n", i, actions[i].key, tag);
  }
  fclose(file);
}

static void _create_logtable(worker_t *w, int class, int id, size_t size) {
  char filename[256];
  snprintf(filename, 256, "event.%03d.%03d.%05d.%s.log",
           w->id, id, hpx_get_my_rank(),
           TRACE_EVENT_TO_STRING[id]);

  char *path = _concat_path(_log_path, filename);
  w->inst->logs[id] = malloc(sizeof(logtable_t));
  logtable_init(w->inst->logs[id], path, size, class, id);
  free(path);
}

static const char *_mkdir(const char *dir) {
  // try and create the directory---we don't care if it's already there
  if (mkdir(dir, 0777) && errno != EEXIST) {
    log_error("Could not create %s for instrumentation\n", dir);
  }
  return dir;
}

static const char *_mktmp(void) {
  // create directory name
  time_t t = time(NULL);
  struct tm lt;
  localtime_r(&t, &lt);
  char dirname[256];
  struct passwd *pwd = getpwuid(getuid());
  const char *username = pwd->pw_name;
  snprintf(dirname, 256, "hpx-%s.%.4d%.2d%.2d.%.2d%.2d", username,
           lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min);
  const char *path = _concat_path("/tmp", dirname);
  log_internal(__LINE__, __FILE__, __func__,
               "logging instrumentation files into %s\n", path);
  return _mkdir(path);
}

static const char *_get_log_path(const char *dir) {
  if (!dir) {
    return _mktmp();
  }

  struct stat sb;
  // if the path doesn't exist, then we make it
  if (stat(dir, &sb) != 0) {
    return _mkdir(dir);
  }

  // if the path is a directory, we just return it
  if (S_ISDIR(sb.st_mode)) {
    return strndup(dir, 256);
  }

  // path exists but isn't a directory.
  log_error("--with-inst-dir=%s does not point to a directory\n", dir);
  return NULL;
}

int inst_init(const config_t *cfg, worker_t *w) {
#ifndef ENABLE_INSTRUMENTATION
  return LIBHPX_OK;
#endif

  // If we're not tracing at this locality, then don't do anything.
  if (!config_trace_at_isset(cfg, HPX_LOCALITY_ID) || !cfg->trace_classes) {
    return LIBHPX_OK;
  }

  // At this point we know that we'll be generating some sort of logs, so
  // prepare the path.
  if (w->id == 0){
    _log_path = _get_log_path(cfg->trace_dir);
  }

  if (!_log_path) {
    return LIBHPX_OK;
  }

  // Allocate memory for pointers to the logs and their respective locks
  int nclasses = _HPX_NELEM(HPX_TRACE_CLASS_TO_STRING);
  w->inst->logs = calloc(TRACE_OFFSETS[nclasses], sizeof(logtable_t *));

  // Scan through each trace event class and create logs for the associated
  // class events that that we are going to be tracing.
  for (int c = 0, e = nclasses; c < e; ++c) {
    if (inst_trace_class(1 << c)) {
      for (int i = TRACE_OFFSETS[c], e = TRACE_OFFSETS[c + 1]; i < e; ++i) {
        _create_logtable(w, c, i, cfg->trace_buffersize);
      }
    }
  }

  return LIBHPX_OK;
}

int inst_start(void) {
  // If we're tracing parcels then we need to output the actions and the
  // hostnames as well. The "_dump" actions won't do anything if we're not
  // instrumenting locally.
  // @todo 1. Shouldn't we dump these for all ranks, even if we're not
  //          instrumenting *at* that rank? Otherwise we'll be missing data for
  //          ranks that *are* instrumenting.
  //       2. Why can't we just do this during initialization?
  if (inst_trace_class(HPX_TRACE_PARCEL)) {
    _dump_actions();
    _dump_hostnames();
  }
  return LIBHPX_OK;
}

void inst_fini(worker_t *w) {
  if (_log_path) {
    free((char*)_log_path);
  }
  _log_path = NULL;
  if(!w->inst->logs){
    return;
  }
  for (int i = 0, e = TRACE_NUM_EVENTS; i < e; ++i) {
    if(w->inst->logs[i]){
      logtable_fini(w->inst->logs[i]);
      free(w->inst->logs[i]);
      w->inst->logs[i] = NULL;
    }
  }
  free(w->inst->logs);
  w->inst->logs = NULL;
}

void inst_vtrace(int UNUSED, int n, int id, ...) {
  if (self && self->inst->logs && self->inst->logs[id]) {
    va_list vargs;
    va_start(vargs, id);
    logtable_vappend(self->inst->logs[id], n, &vargs);
    va_end(vargs);
  }
}
