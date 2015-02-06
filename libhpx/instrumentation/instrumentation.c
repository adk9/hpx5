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
# include "config.h"
#endif

#include <inttypes.h> // PRIuXX
#include <stdio.h>
#include <stdlib.h> // atoi and getenv
#include <string.h>
#include <time.h> // time()

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

//#include <dlfcn.h> // dlsym

#include <hpx/hpx.h>
#include <libsync/sync.h>
#include "libhpx/debug.h"
#include "libhpx/instrumentation.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/parcel.h"
#include "logtable.h"

#ifdef ENABLE_INSTRUMENTATION
static logtable_t logtables[HPX_INST_NUM_EVENTS];
static size_t inst_max_log_size = 40*1024*1024-1;
bool hpx_inst_enabled = false;
bool hpx_inst_parcel_enabled = false;
static hpx_time_t time_start;
static bool inst_active = false; // not whether we WANT to log, but whether
                                 // we are ABLE to (i.e. after initialization
                                 // and before shutdown)

static bool get_env_var(char *env_var_name) {
  char* env_var_text;
  env_var_text = getenv(env_var_name);
  if (env_var_text != NULL && atoi(env_var_text))
    return true;
  else
    return false;
}

static void
time_diff(uint64_t *out_s, uint64_t *out_ns, hpx_time_t *start, hpx_time_t *end) {
  {
    if ((end->tv_nsec-start->tv_nsec)<0) {
      *out_s = end->tv_sec-start->tv_sec-1;
      *out_ns = (1e9+end->tv_nsec)-start->tv_nsec;
    } else {
      *out_s = end->tv_sec-start->tv_sec;
      *out_ns = end->tv_nsec-start->tv_nsec;
    }
  }
}

static int
_log_create(hpx_inst_class_type_t class, hpx_inst_event_type_t event,
               size_t max_size) {
  char filename[256];
  snprintf(filename, 256, "event.%d.%d.%d.%s.%s.log",
           class, event, hpx_get_my_rank(),
           HPX_INST_CLASS_TYPE_TO_STRING[class],
           HPX_INST_EVENT_TYPE_TO_STRING[event]);

  int success = logtable_init(&logtables[event], filename, max_size);
  if (success != HPX_ERROR) {
    return success;
  }
  return HPX_SUCCESS;
}

static int _enabled(hpx_inst_class_type_t class) {
  return (here->config->traceclasses & (1 << (class + 1)));
}

#endif

int hpx_inst_init() {
#ifdef ENABLE_INSTRUMENTATION
  int success = -1;

  hpx_inst_enabled = get_env_var("HPX_INST_ENABLE");
  if (!hpx_inst_enabled) {
    return HPX_SUCCESS;
  }

  char* max_log_size_text;
  max_log_size_text = getenv("HPX_INST_MAX_LOG_SIZE");
  if (max_log_size_text != NULL && atoll(max_log_size_text) > 0) {
    inst_max_log_size = (size_t)atoll(max_log_size_text);
  }

  // change to user-specified root directory
  const char* inst_rootdir = getenv("HPX_INST_ROOTDIR");
  if (0 != chdir(inst_rootdir)) {
    dbg_error("Specified root directory for instrumentation not found.");
  }

  // create directory name
  time_t t = time(NULL);
  struct tm lt;
  localtime_r(&t, &lt);
  char inst_dir_name[256];
  snprintf(inst_dir_name, 256, "hpx.%.2d%.2d.%.2d%.2d%.4d",
           lt.tm_hour, lt.tm_min, lt.tm_mday, lt.tm_mon + 1,
           lt.tm_year + 1900);

  // try and create the directory---we don't care if it's already there
  success = mkdir(inst_dir_name, 0777);
  if (success == 0 || EEXIST == success) {
    success = chdir(inst_dir_name);
    if (success != 0) {
      dbg_error("could not change directories\n");
    }
  }
  else {
    dbg_error("Could not create %s for instrumentation.\n", inst_dir_name);
  }

  // create log files
  hpx_inst_parcel_enabled = get_env_var("HPX_INST_PARCELS");

  for (int class = 0, e = HPX_INST_NUM_CLASSES; class < e; ++class) {
    if (!_enabled(class)) {
      continue;
    }
    int event = HPX_INST_CLASS_EVENT_OFFSET[class];
    int e = HPX_INST_CLASS_EVENT_OFFSET[class + 1];
    for (; event < e; ++event) {
      int e = _log_create(class, event, inst_max_log_size);
      if (LIBHPX_OK != e) {
        dbg_error("failed to create a log for %s\n",
                  HPX_INST_EVENT_TYPE_TO_STRING[event]);
      }
    }
  }

  time_start = hpx_time_now();

  inst_active = true;
#endif
  return HPX_SUCCESS;
}

void hpx_inst_fini() {
#ifdef ENABLE_INSTRUMENTATION
  inst_active = false;
  for (int i = 0; i < HPX_INST_NUM_EVENTS; i++) {
    if (logtables[i].inited == true) {
      logtable_fini(&logtables[i]);
    }
  }
#endif
}

/// Record an event to the log
/// @param class          Class this event is part of (see
///                       hpx_inst_class_type_t)
/// @param event_type     The type of this event (see hpx_inst_event_type_t)
/// @param priority       The priority of this event (may be filtered out if
///                       below some threshhold)
/// @param user_data_size The size of the data to record
/// @param user_data      The data to record (is copied)
void hpx_inst_log_event(hpx_inst_class_type_t class,
                        hpx_inst_event_type_t event_type,
                        int priority,
                        int user_data_size,
                        void* user_data) {
#ifdef ENABLE_INSTRUMENTATION
  if (!_enabled(class)) {
    return;
  }

  if (!inst_active) {
    return;
  }

  logtable_t *lt = &logtables[event_type];

  hpx_inst_event_t* event = logtable_next_and_increment(lt);
  if (event == NULL) {
    return;
  }

  event->class = class;
  event->event_type = event_type;
  event->rank = hpx_get_my_rank();
  event->worker = hpx_get_my_thread_id();
  //  event->thread = hpx_thread_get_tls_id();
  hpx_time_t time_now = hpx_time_now();
  time_diff(&event->s, &event->ns, &time_start, &time_now);

  //  event->priority = priority;

  // generate random id? (can't just increment since we're
  // distributed) (also can't use time because small change two events
  // could line up on different ranks)
  //  event->id = id;

  memcpy(event->data, user_data, user_data_size);

#endif
}

