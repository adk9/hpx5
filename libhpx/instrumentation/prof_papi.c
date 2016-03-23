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
#include <papi.h>
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

static uint64_t _papi_events[] = {
  [HPX_L1_TCM]  = PAPI_L1_TCM,
  [HPX_L1_TCA]  = PAPI_L1_TCA,
  [HPX_L2_TCM]  = PAPI_L2_TCM,
  [HPX_L2_TCA]  = PAPI_L2_TCA,
  [HPX_L3_TCM]  = PAPI_L3_TCM,
  [HPX_L3_TCA]  = PAPI_L3_TCA,
  [HPX_TLB_TL]  = PAPI_TLB_TL,
  [HPX_TOT_INS] = PAPI_TOT_INS,
  [HPX_INT_INS] = PAPI_INT_INS,
  [HPX_FP_INS]  = PAPI_FP_INS,
  [HPX_LD_INS]  = PAPI_LD_INS,
  [HPX_SR_INS]  = PAPI_SR_INS,
  [HPX_BR_INS]  = PAPI_BR_INS,
  [HPX_TOT_CYC] = PAPI_TOT_CYC,
};

static int _set_event(int id, uint64_t papi_event, uint64_t event) {
  if (PAPI_query_event(papi_event) != PAPI_OK) {
    const char *counter_name = HPX_COUNTER_TO_STRING[event];
    log_dflt("Warning: %s is not available on this system\n", counter_name);
    return 0;
  }

  profile_log.counters[id] = event;
  return 1;
}

int prof_init(config_t *cfg) {
  // initialize PAPI
  int e = PAPI_library_init(PAPI_VER_CURRENT);
  if (e != PAPI_VER_CURRENT) {
    log_error("unable to initialize PAPI (error code %d)\n", e);
  }

  // get the number of available hardware counters on the platform
  int max_counters = PAPI_num_counters();

  // get the number of counters requested by the user
  int counters = cfg->prof_counters;
  int req_counters = popcountl(counters);

  if (req_counters - (HPX_TIMERS & counters) > max_counters) {
    log_dflt("WARNING: maximum available counters is %d\n", max_counters);
    req_counters = max_counters + (HPX_PROF_TIMERS & counters);
  }

  profile_log.counters = calloc(req_counters, sizeof(int));
  profile_log.events = calloc(profile_log.max_events, sizeof(profile_list_t));

  int num_counters = 0;
  for (int c = 0; c < HPX_COUNTER_MAX && num_counters < req_counters; ++c) {
    if ((UINT64_C(1) << c) & counters) {
      if (c) {
        uint64_t papi_event = _papi_events[c];
        num_counters += _set_event(num_counters, papi_event, c);
      }
      else {
        profile_log.counters[num_counters] = HPX_TIMERS;
        num_counters++;
      }
    }
  }

  profile_log.num_counters = num_counters;
  return LIBHPX_OK;
}

void prof_fini(void) {
  inst_prof_dump(profile_log);
  for (int i = 0; i < profile_log.num_events; i++) {
    if (!profile_log.events[i].simple) {
      for (int j = 0; j < profile_log.events[i].num_entries; j++) {
        free(profile_log.events[i].entries[j].counter_totals);
      }
    }
    free(profile_log.events[i].entries);
  }
  free(profile_log.events);
  free(profile_log.counters);
}

int prof_get_averages(int64_t *values, char *key) {
  int event = profile_get_event(key);
  if (event < 0 || profile_log.events[event].simple) {
    return LIBHPX_EINVAL;
  }

  for (int i = 0; i < profile_log.num_counters; i++) {
    values[i] = 0;
    double divisor = 0;

    // 'use' variable is necessary for offsetting as timing is not included in
    // the 'counter_totals' array, but is considered in 'num_counters'
    int use = i - (HPX_TIMERS == profile_log.counters[0]) ? 1 : 0;

    if (use < 0) {
      for (int j = 0; j < profile_log.events[event].num_entries; j++) {
        if (profile_log.events[event].entries[j].marked) {
          values[i] += hpx_time_diff_ns(HPX_TIME_NULL,
                           profile_log.events[event].entries[j].run_time);
          divisor++;
        }
      }
    }
    else {
      for (int j = 0; j < profile_log.events[event].num_entries; j++) {
        if (profile_log.events[event].entries[j].marked &&
            profile_log.events[event].entries[j].counter_totals[use] >= 0) {
          values[i] += profile_log.events[event].entries[j].counter_totals[i];
          divisor++;
        }
      }
    }
    if (divisor > 0) {
      values[i] /= divisor;
    }
  }

  return LIBHPX_OK;
}

int prof_get_totals(int64_t *values, char *key) {
  int event = profile_get_event(key);
  if (event < 0 || profile_log.events[event].simple) {
    return LIBHPX_EINVAL;
  }

  for (int i = 0; i < profile_log.num_counters; i++) {
    values[i] = 0;
  
    // 'use' variable is necessary for offsetting as timing is not included in
    // the 'counter_totals' array, but is considered in 'num_counters'
    int use = i - (HPX_TIMERS == profile_log.counters[0]) ? 1 : 0;

    if (use < 0) {
      for (int j = 0; j < profile_log.events[event].num_entries; j++) {
        if (profile_log.events[event].entries[j].marked) {
          values[i] += hpx_time_diff_ns(HPX_TIME_NULL,
                           profile_log.events[event].entries[j].run_time);
        }
      }
    }
    else {
      for (int j = 0; j < profile_log.events[event].num_entries; j++) {
        if (profile_log.events[event].entries[j].marked &&
            profile_log.events[event].entries[j].counter_totals[use] >= 0) {
          values[i] += profile_log.events[event].entries[j].counter_totals[i];
        }
      }
    }
  }
  return LIBHPX_OK;
}

int prof_get_minimums(int64_t *values, char *key) {
  int event = profile_get_event(key);
  if (event < 0 ||profile_log.events[event].simple) {
    return LIBHPX_EINVAL;
  }

  for (int i = 0; i < profile_log.num_counters; i++) {
    values[i] = 0;
    int start = profile_log.events[event].num_entries;
    int64_t temp;
    
    // 'use' variable is necessary for offsetting as timing is not included in
    // the 'counter_totals' array, but is considered in 'num_counters'
    int use = i - (HPX_TIMERS == profile_log.counters[0]) ? 1 : 0;

    if (use < 0) {
      for (int j = 0; j < profile_log.events[event].num_entries; j++) {
        if (profile_log.events[event].entries[i].marked) {
          values[i] = hpx_time_diff_ns(HPX_TIME_NULL, 
                          profile_log.events[event].entries[j].run_time);
          start = j + 1;
          break;
        }
      }
      for (int j = start; j < profile_log.events[event].num_entries; j++) {
        if (profile_log.events[event].entries[j].marked) {
          temp = hpx_time_diff_ns(HPX_TIME_NULL,
                     profile_log.events[event].entries[j].run_time);
          if (temp < values[i] && temp >= 0) {
            values[i] = temp;
          }
        }
      }
    }
    else {
      for (int j = 0; j < profile_log.events[event].num_entries; j++) {
        if (profile_log.events[event].entries[i].marked) {
          values[i] = profile_log.events[event].entries[j].counter_totals[use];
          start = j + 1;
          break;
        }
      }
      for (int j = start; j < profile_log.events[event].num_entries; j++) {
        if (profile_log.events[event].entries[j].marked) {
          temp = profile_log.events[event].entries[j].counter_totals[use];
          if (temp < values[i] && temp >= 0) {
            values[i] = temp;
          }
        }
      }
    }
  }
  return LIBHPX_OK;
}

int prof_get_maximums(int64_t *values, char *key) {
  int event = profile_get_event(key);
  if (event < 0 ||profile_log.events[event].simple) {
    return LIBHPX_EINVAL;
  }

  for (int i = 0; i < profile_log.num_counters; i++) {
    values[i] = 0;
    int start = profile_log.events[event].num_entries;
    int64_t temp;
    
    // 'use' variable is necessary for offsetting as timing is not included in
    // the 'counter_totals' array, but is considered in 'num_counters'
    int use = i - (HPX_TIMERS == profile_log.counters[0]) ? 1 : 0;
    
    if (use < 0) {
      for (int j = 0; j < profile_log.events[event].num_entries; j++) {
        if (profile_log.events[event].entries[i].marked) {
          values[i] = hpx_time_diff_ns(HPX_TIME_NULL, 
                          profile_log.events[event].entries[j].run_time);
          start = j + 1;
          break;
        }
      }
      for (int j = start; j < profile_log.events[event].num_entries; j++) {
        if (profile_log.events[event].entries[j].marked) {
          temp = hpx_time_diff_ns(HPX_TIME_NULL,
                     profile_log.events[event].entries[j].run_time);
          if (temp < values[i] && temp >= 0) {
            values[i] = temp;
          }
        }
      }
    }
    else {
      for (int j = 0; j < profile_log.events[event].num_entries; j++) {
        if (profile_log.events[event].entries[i].marked) {
          values[i] = profile_log.events[event].entries[j].counter_totals[use];
          start = j + 1;
          break;
        }
      }
      for (int j = start; j < profile_log.events[event].num_entries; j++) {
        if (profile_log.events[event].entries[j].marked) {
          temp = profile_log.events[event].entries[j].counter_totals[use];
          if (temp > values[i]) {
            values[i] = temp;
          }
        }
      }
    }
  }
  return LIBHPX_OK;
}

static int _create_new_event(char *key) {
  int eventset = PAPI_NULL;
  int retval = PAPI_create_eventset(&eventset);
  if (retval != PAPI_OK) {
    log_error("unable to create eventset with error code %d\n", retval);
  } else {
    for (int i = (profile_log.counters[0] == HPX_TIMERS) ? 1 : 0; 
         i < profile_log.num_counters; i++) {
      PAPI_add_event(eventset, _papi_events[profile_log.counters[i]]);
    }
  }

  return profile_new_event(key, false, eventset);
}

int prof_start_hardware_counters(char *key, int *tag) {
  hpx_time_t end = 
      (HPX_TIMERS == profile_log.counters[0]) ? hpx_time_now() : HPX_TIME_NULL;

  int event = profile_get_event(key);
  if (event < 0) {
    event = _create_new_event(key);
  }
  if(event < 0){
    return LIBHPX_ERROR;
  }

  if (profile_log.events[event].simple) {
    return LIBHPX_EINVAL;
  }
  
  int how_many = profile_log.num_counters - 
                 (HPX_TIMERS == profile_log.counters[0]) ? 1 : 0;
  
  // update the current event and entry being recorded
  if (profile_log.current_event >= 0 && 
     !profile_log.events[profile_log.current_event].entries[
                          profile_log.current_entry].marked &&
     !profile_log.events[profile_log.current_event].entries[
                          profile_log.current_entry].paused) {

    // left as long long instead of int64_t to suppress a warning
    // at compile time
    long long values[how_many];

    for (int i = 0; i < how_many; i++) {
      values[i] = -1;
    }
    PAPI_stop(profile_log.events[profile_log.current_event].eventset, values);

    if (how_many != profile_log.num_counters) {
      hpx_time_t dur;
      hpx_time_diff(profile_log.events[
                    profile_log.current_event].entries[
                    profile_log.current_entry].ref_time, end, &dur);
                    profile_log.events[profile_log.current_event].entries[
                    profile_log.current_entry].run_time =
             hpx_time_add(profile_log.events[profile_log.current_event].entries[
                          profile_log.current_entry].run_time, dur);
    }

    for (int i = 0; i < how_many; i++) {
      profile_log.events[profile_log.current_event].entries[
                         profile_log.current_entry].counter_totals[i] 
                           += (int64_t) values[i];
    }
  }

  int index = profile_new_entry(event);
  
  profile_log.events[event].entries[index].last_entry = profile_log.current_entry;
  profile_log.events[event].entries[index].last_event = profile_log.current_event;
  profile_log.current_entry = index;
  profile_log.current_event = event;
  *tag = index;
  PAPI_reset(profile_log.events[profile_log.current_event].eventset);
  
  profile_log.events[event].entries[index].start_time = hpx_time_now();
  if (how_many != profile_log.num_counters){
    profile_log.events[event].entries[index].ref_time = 
      profile_log.events[event].entries[index].start_time;
  }
  return PAPI_start(profile_log.events[event].eventset);
}

int prof_stop_hardware_counters(char *key, int *tag) {
  hpx_time_t end = 
      (HPX_TIMERS == profile_log.counters[0]) ? hpx_time_now() : HPX_TIME_NULL;

  int event = profile_get_event(key);
  if (event < 0 || *tag == HPX_PROF_NO_TAG) {
    return LIBHPX_EINVAL;
  }

  int how_many = profile_log.num_counters - 
                 (HPX_TIMERS == profile_log.counters[0]) ? 1 : 0;

  // left as long long instead of int64_t to suppress a warning
  // at compile time
  long long values[how_many];
  
  for (int i = 0; i < how_many; i++) {
    values[i] = -1;
  }
  int retval = PAPI_stop(profile_log.events[event].eventset, values);
  PAPI_reset(profile_log.events[event].eventset);
  if (retval != PAPI_OK) {
    return retval;
  }
  
  for (int i = 0; i < how_many; i++) {
    profile_log.events[event].entries[*tag].counter_totals[i] 
      += (int64_t) values[i];
  }

  int index = profile_log.current_entry;
  profile_log.current_entry = profile_log.events[event].entries[index].last_entry;
  profile_log.current_event = profile_log.events[event].entries[index].last_event;

  // if another event/entry was being measured prior to switching to the current
  // event/entry, then pick up where we left off (check current_event because it
  // was already updated in prof_stop_timing())
  if (profile_log.current_event >= 0 && 
     !profile_log.events[event].entries[profile_log.current_entry].paused) {

    if (how_many != profile_log.num_counters) {
      profile_log.events[event].entries[profile_log.current_entry].ref_time = 
        profile_log.events[event].entries[profile_log.current_entry].start_time;
    }
    PAPI_start(profile_log.events[profile_log.current_event].eventset);
  }

  return LIBHPX_OK;
}

int prof_pause(char *key, int *tag) {
  hpx_time_t end = 
      (HPX_TIMERS == profile_log.counters[0]) ? hpx_time_now() : HPX_TIME_NULL;

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
  if (profile_log.counters[0] == HPX_TIMERS) {
    hpx_time_t dur;
    hpx_time_diff(profile_log.events[event].entries[*tag].ref_time, end, &dur);
    profile_log.events[event].entries[*tag].run_time = 
      hpx_time_add(profile_log.events[event].entries[*tag].run_time, dur);
  }

  // then store counter information if necessary
  if (!profile_log.events[event].simple) {
    int how_many = profile_log.num_counters - 
                   (HPX_TIMERS == profile_log.counters[0]) ? 1 : 0;

    // I leave this as type long long instead of int64_t to suppress a warning
    // at compile time that appears if I do otherwise
    long long values[how_many];
  
    for (int i = 0; i < how_many; i++) {
      values[i] = -1;
    }
    int retval = PAPI_stop(profile_log.events[event].eventset, values);
    PAPI_reset(profile_log.events[event].eventset);
    if (retval != PAPI_OK) {
      return retval;
    }
    
    for (int i = 0; i < how_many; i++) {
      profile_log.events[event].entries[*tag].counter_totals[i] 
        += (int64_t) values[i];
    }
  }
  profile_log.events[event].entries[*tag].paused = true;
  return LIBHPX_OK;
}

int prof_resume(char *key, int *tag) {
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
  if (profile_log.counters[0] == HPX_TIMERS) {
    profile_log.events[event].entries[*tag].ref_time = hpx_time_now();
  }
  if (!profile_log.events[event].simple) {
    PAPI_reset(profile_log.events[profile_log.current_event].eventset);
    return PAPI_start(profile_log.events[event].eventset);
  }
  return LIBHPX_OK;
}
