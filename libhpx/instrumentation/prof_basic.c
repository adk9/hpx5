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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <hpx/hpx.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/instrumentation.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/profiling.h>
#include <libsync/sync.h>

#include "profile.h"

int prof_init(const config_t *cfg) {
  profile_log.counters = NULL;
  profile_log.num_counters = 0;
  if (config_prof_counters_isset(cfg, HPX_PROF_TIMERS)) {
    profile_log.num_counters = 1;
    profile_log.counters = malloc(sizeof(int));
    profile_log.counters[0] = HPX_TIMERS;
  }
  profile_log.events = malloc(profile_log.max_events * sizeof(profile_list_t));
  dbg_assert(profile_log.events);
  return LIBHPX_OK;
}

void prof_fini(void) {
  inst_prof_dump(&profile_log);
  if (profile_log.num_counters > 0) {
    free(profile_log.counters);
  }
  for (int i = 0, e = profile_log.num_events; i < e; ++i) {
    free(profile_log.events[i].entries);
  }
  free(profile_log.events);
}

int prof_get_averages(int64_t *values, char *key) {
  return LIBHPX_OK;
}

int prof_get_totals(int64_t *values, char *key) {
  return LIBHPX_OK;
}

int prof_get_minimums(int64_t *values, char *key) {
  return LIBHPX_OK;
}

int prof_get_maximums(int64_t *values, char *key) {
  return LIBHPX_OK;
}

int prof_start_hardware_counters(char *key, int *tag) {
  if (profile_log.num_counters == 0) {
    return LIBHPX_OK;
  }

  prof_start_timing(key, tag);
  return LIBHPX_OK;
}

int prof_stop_hardware_counters(char *key, int *tag) {
  if (profile_log.num_counters == 0) {
    return LIBHPX_OK;
  }

  prof_stop_timing(key, tag);
  return LIBHPX_OK;
}

int prof_pause(char *key, int *tag) {
  if (profile_log.num_counters == 0) {
    return LIBHPX_OK;
  }

  hpx_time_t end = hpx_time_now();
  int event = profile_get_event(key);
  if (event < 0) {
    return LIBHPX_EINVAL;
  }

  if (*tag == HPX_PROF_NO_TAG) {
    for (int i = profile_log.events[event].num_entries - 1; i >= 0; i--) {
      if (!profile_log.events[event].entries[i].marked &&
         !profile_log.events[event].entries[i].paused) {
        *tag = i;
        break;
      }
    }
  }
  if (*tag == HPX_PROF_NO_TAG ||
     profile_log.events[event].entries[*tag].marked ||
     profile_log.events[event].entries[*tag].paused) {
    return LIBHPX_EINVAL;
  }

  // first store timing information
  hpx_time_t dur;
  hpx_time_diff(profile_log.events[event].entries[*tag].ref_time, end, &dur);
  profile_log.events[event].entries[*tag].run_time =
      hpx_time_add(profile_log.events[event].entries[*tag].run_time, dur);

  profile_log.events[event].entries[*tag].paused = true;
  return LIBHPX_OK;
}

int prof_resume(char *key, int *tag) {
  if (profile_log.num_counters == 0) {
    return LIBHPX_OK;
  }

  int event = profile_get_event(key);
  if (event < 0) {
    return LIBHPX_EINVAL;
  }

  if (*tag == HPX_PROF_NO_TAG) {
    for (int i = profile_log.events[event].num_entries - 1; i >= 0; i--) {
      if (!profile_log.events[event].entries[i].marked &&
         profile_log.events[event].entries[i].paused) {
        *tag = i;
        break;
      }
    }
  }
  if (*tag == HPX_PROF_NO_TAG) {
    return LIBHPX_EINVAL;
  }

  profile_log.events[event].entries[*tag].paused = false;
  profile_log.events[event].entries[*tag].start_time = hpx_time_now();
  profile_log.events[event].entries[*tag].ref_time =
    profile_log.events[event].entries[*tag].start_time;
  return LIBHPX_OK;
}

