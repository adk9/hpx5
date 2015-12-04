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

/// Each locality maintains a single profile log
extern profile_log_t _profile_log;

static const char* _get_counter_string(uint64_t papi_event) {
  switch(papi_event) {
    case PAPI_L1_TCM:  return "PAPI_L1_TCM";
    case PAPI_L2_TCM:  return "PAPI_L2_TCM";
    case PAPI_L3_TCM:  return "PAPI_L3_TCM";
    case PAPI_TLB_TL:  return "PAPI_TLB_TL";
    case PAPI_TOT_INS: return "PAPI_TOT_INS";
    case PAPI_INT_INS: return "PAPI_INT_INS";
    case PAPI_FP_INS:  return "PAPI_FP_INS";
    case PAPI_LD_INS:  return "PAPI_LD_INS";
    case PAPI_SR_INS:  return "PAPI_SR_INS";
    case PAPI_BR_INS:  return "PAPI_BR_INS";
    case PAPI_TOT_CYC: return "PAPI_TOT_CYC";
    default:           return "PAPI counter";
  }
}

static void _set_event(uint64_t papi_event, uint64_t bit, uint64_t bitset, 
                       int max_counters, int *num_counters) {
  if (!(bit & bitset) || (PAPI_query_event(papi_event) != PAPI_OK)
                     || (*num_counters >= max_counters)) {
    return;
  }

  _profile_log.counters[*num_counters] = papi_event;
  *num_counters+=1;
}

static void _test_event(uint64_t papi_event, uint64_t bit, uint64_t bitset,
                        int max_counters, int *num_counters) {
  if (!(bit & bitset)) {
    return;
  }

  if (PAPI_query_event(papi_event) != PAPI_OK) {
    const char *counter_name = _get_counter_string(papi_event);
    log_error("Warning: %s is not available on this system\n", counter_name);
    return;
  }

  *num_counters+=1;
  if (*num_counters > max_counters) {
    const char *counter_name = _get_counter_string(papi_event);
    log_error("Warning: %s could not be included in profiling due to limited "
              "resources\n", counter_name);
  }
}

static int _create_new_event(char *key) {
  int eventset = PAPI_NULL;
  int retval = PAPI_create_eventset(&eventset);
  if (retval != PAPI_OK) {
    log_error("unable to create eventset with error code %d\n", retval);
  } else {
    for (int i = 0; i < _profile_log.num_counters; i++) {
      PAPI_add_event(eventset, _profile_log.counters[i]);
    }
  }

  return profile_new_event(key, false, eventset);
}

int prof_init(config_t *cfg) {
  int max_counters = PAPI_num_counters();
  int num_counters = 0;
  uint64_t counters = cfg->prof_counters;

  int retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT) {
    log_error("unable to initialize PAPI with error code %d\n", retval);
  }

  //test events first to generate warnings and determine where counters
  //can have space allocated for them
  _test_event(PAPI_L1_TCM, HPX_PAPI_L1_TCM, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_L2_TCM, HPX_PAPI_L2_TCM, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_L3_TCM, HPX_PAPI_L3_TCM, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_TLB_TL, HPX_PAPI_TLB_TL, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_TOT_INS, HPX_PAPI_TOT_INS, counters,
              max_counters, &num_counters);
  _test_event(PAPI_INT_INS, HPX_PAPI_INT_INS, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_FP_INS, HPX_PAPI_FP_INS, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_LD_INS, HPX_PAPI_LD_INS, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_SR_INS, HPX_PAPI_SR_INS, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_BR_INS, HPX_PAPI_BR_INS, counters, 
              max_counters, &num_counters);
  _test_event(PAPI_TOT_CYC, HPX_PAPI_TOT_CYC, counters, 
              max_counters, &num_counters);

  if (num_counters > max_counters) {
    log_error("Note: maximum available counters is %d\n", max_counters);
    num_counters = max_counters;
  }

  _profile_log.counters = malloc(num_counters * sizeof(int));
  _profile_log.num_counters = num_counters;
  _profile_log.events = malloc(_profile_log.max_events * 
                                sizeof(profile_list_t));

  max_counters = num_counters;
  num_counters = 0;

  //actually set the events
  _set_event(PAPI_L1_TCM, HPX_PAPI_L1_TCM, counters, 
             max_counters, &num_counters);  
  _set_event(PAPI_L2_TCM, HPX_PAPI_L2_TCM, counters,
             max_counters, &num_counters);  
  _set_event(PAPI_L3_TCM, HPX_PAPI_L3_TCM, counters,
             max_counters, &num_counters);  
  _set_event(PAPI_TLB_TL, HPX_PAPI_TLB_TL, counters,
             max_counters, &num_counters);  
  _set_event(PAPI_TOT_INS, HPX_PAPI_TOT_INS, counters,
             max_counters, &num_counters);
  _set_event(PAPI_INT_INS, HPX_PAPI_INT_INS, counters,
             max_counters, &num_counters);
  _set_event(PAPI_FP_INS, HPX_PAPI_FP_INS, counters,
             max_counters, &num_counters);  
  _set_event(PAPI_LD_INS, HPX_PAPI_LD_INS, counters,
             max_counters, &num_counters);  
  _set_event(PAPI_SR_INS, HPX_PAPI_SR_INS, counters,
             max_counters, &num_counters);  
  _set_event(PAPI_BR_INS, HPX_PAPI_BR_INS, counters,
             max_counters, &num_counters);  
  _set_event(PAPI_TOT_CYC, HPX_PAPI_TOT_CYC, counters,
             max_counters, &num_counters);
  return LIBHPX_OK;
}

void prof_fini(void) {
  inst_prof_dump(_profile_log);
  for (int i = 0; i < _profile_log.num_events; i++) {
    if (!_profile_log.events[i].simple) {
      for (int j = 0; j < _profile_log.events[i].num_entries; j++) {
        free(_profile_log.events[i].entries[j].counter_totals);
      }
    }
    free(_profile_log.events[i].entries);
  }
  free(_profile_log.events);
  free(_profile_log.counters);
}

int prof_get_averages(int64_t *values, char *key) {
  int event = profile_get_event(key);
  if (event < 0 ||_profile_log.events[event].simple) {
    return PAPI_EINVAL;
  }

  for (int i = 0; i < _profile_log.num_counters; i++) {
    values[i] = 0;
    double divisor = 0;
    for (int j = 0; j < _profile_log.events[event].num_entries; j++) {
      if (_profile_log.events[event].entries[j].marked &&
         _profile_log.events[event].entries[j].counter_totals[i] >= 0) {
        values[i] += _profile_log.events[event].entries[j].counter_totals[i];
        divisor++;
      }
    }
    if (divisor > 0) {
      values[i] /= divisor;
    }
  }
  return PAPI_OK;
}

int prof_get_totals(int64_t *values, char *key) {
  int event = profile_get_event(key);
  if (event < 0 || _profile_log.events[event].simple) {
    return PAPI_EINVAL;
  }

  for (int i = 0; i < _profile_log.num_counters; i++) {
    values[i] = 0;
    for (int j = 0; j < _profile_log.events[event].num_entries; j++) {
      if (_profile_log.events[event].entries[j].marked &&
         _profile_log.events[event].entries[j].counter_totals[i] >= 0) {
        values[i] += _profile_log.events[event].entries[j].counter_totals[i];
      }
    }
  }
  return PAPI_OK;
}

int prof_get_minimums(int64_t *values, char *key) {
  int event = profile_get_event(key);
  if (event < 0 ||_profile_log.events[event].simple) {
    return PAPI_EINVAL;
  }

  for (int i = 0; i < _profile_log.num_counters; i++) {
    values[i] = 0;
    int start = _profile_log.events[event].num_entries;
    int64_t temp;
    for (int j = 0; j < _profile_log.events[event].num_entries; j++) {
      if (_profile_log.events[event].entries[i].marked) {
        values[i] = _profile_log.events[event].entries[j].counter_totals[i];
        start = j + 1;
        break;
      }
    }
    for (int j = start; j < _profile_log.events[event].num_entries; j++) {
      if (_profile_log.events[event].entries[j].marked) {
        temp = _profile_log.events[event].entries[j].counter_totals[i];
        if (temp < values[i] && temp >= 0) {
          values[i] = temp;
        }
      }
    }
  }
  return PAPI_OK;
}

int prof_get_maximums(int64_t *values, char *key) {
  int event = profile_get_event(key);
  if (event < 0 ||_profile_log.events[event].simple) {
    return PAPI_EINVAL;
  }

  for (int i = 0; i < _profile_log.num_counters; i++) {
    values[i] = 0;
    int start = _profile_log.events[event].num_entries;
    int64_t temp;
    for (int j = 0; j < _profile_log.events[event].num_entries; j++) {
      if (_profile_log.events[event].entries[i].marked) {
        values[i] = _profile_log.events[event].entries[j].counter_totals[i];
        start = j + 1;
        break;
      }
    }
    for (int j = start; j < _profile_log.events[event].num_entries; j++) {
      if (_profile_log.events[event].entries[j].marked) {
        temp = _profile_log.events[event].entries[j].counter_totals[i];
        if (temp > values[i]) {
          values[i] = temp;
        }
      }
    }
  }
  return PAPI_OK;
}

int prof_start_hardware_counters(char *key, int *tag) {
  hpx_time_t end = hpx_time_now();
  int event = profile_get_event(key);
  if (event < 0) {
    event = _create_new_event(key);
  }
  dbg_assert(event >= 0);

  if (_profile_log.events[event].simple) {
    return PAPI_EINVAL;
  }
  
  // update the current event and entry being recorded
  if (_profile_log.current_event >= 0 && 
     !_profile_log.events[_profile_log.current_event].entries[
                          _profile_log.current_entry].marked &&
     !_profile_log.events[_profile_log.current_event].entries[
                          _profile_log.current_entry].paused) {
    // left as long long instead of int64_t to suppress a warning
    // at compile time
    long long values[_profile_log.num_counters];
    for (int i = 0; i < _profile_log.num_counters; i++) {
      values[i] = -1;
    }
    PAPI_stop(_profile_log.events[_profile_log.current_event].eventset, 
              values);

    hpx_time_t dur;
    hpx_time_diff(_profile_log.events[
                  _profile_log.current_event].entries[
                  _profile_log.current_entry].start_time, end, &dur);
    _profile_log.events[_profile_log.current_event].entries[
                        _profile_log.current_entry].run_time =
             hpx_time_add(_profile_log.events[_profile_log.current_event].entries[
                        _profile_log.current_entry].run_time, dur);

    for (int i = 0; i < _profile_log.num_counters; i++) {
      _profile_log.events[_profile_log.current_event].entries[
                          _profile_log.current_entry].counter_totals[i] 
                          += (int64_t) values[i];
    }
  }

  int index = profile_new_entry(event);
  
  _profile_log.events[event].entries[index].last_entry = _profile_log.current_entry;
  _profile_log.events[event].entries[index].last_event = _profile_log.current_event;
  _profile_log.current_entry = index;
  _profile_log.current_event = event;
  *tag = index;
  PAPI_reset(_profile_log.events[_profile_log.current_event].eventset);
  _profile_log.events[event].entries[index].start_time = hpx_time_now();
  return PAPI_start(_profile_log.events[event].eventset);
}

int prof_stop_hardware_counters(char *key, int *tag) {
  int event = prof_stop_timing(key, tag);
  if (event < 0 || *tag == HPX_PROF_NO_TAG) {
    return PAPI_EINVAL;
  }

  // left as long long instead of int64_t to suppress a warning
  // at compile time
  long long values[_profile_log.num_counters];
  for (int i = 0; i < _profile_log.num_counters; i++) {
    values[i] = -1;
  }
  int retval = PAPI_stop(_profile_log.events[event].eventset, values);
  PAPI_reset(_profile_log.events[event].eventset);
  if (retval != PAPI_OK) {
    return retval;
  }
  
  for (int i = 0; i < _profile_log.num_counters; i++) {
    _profile_log.events[event].entries[*tag].counter_totals[i] 
      += (int64_t) values[i];
  }

  // if another event/entry was being measured prior to switching to the current
  // event/entry, then pick up where we left off (check current_event because it
  // was already updated in prof_stop_timing())
  if (_profile_log.current_event >= 0 && 
     !_profile_log.events[event].entries[_profile_log.current_entry].paused) {
    PAPI_start(_profile_log.events[_profile_log.current_event].eventset);
  }

  return PAPI_OK;
}

int prof_pause(char *key, int *tag) {
  hpx_time_t end = hpx_time_now();
  int event = profile_get_event(key);
  if (event < 0) {
    return PAPI_EINVAL;
  }

  if (*tag == HPX_PROF_NO_TAG) {
    for (int i = _profile_log.events[event].num_entries - 1; i >= 0; i--) {
      if (!_profile_log.events[event].entries[i].marked && 
         !_profile_log.events[event].entries[i].paused) {
        *tag = i;
        break;
      }
    }
  }
  if (*tag == HPX_PROF_NO_TAG ||
     _profile_log.events[event].entries[*tag].marked ||
     _profile_log.events[event].entries[*tag].paused) {
    return PAPI_EINVAL;
  }

  // first store timing information
  hpx_time_t dur;
  hpx_time_diff(_profile_log.events[event].entries[*tag].start_time, end, &dur);
  _profile_log.events[event].entries[*tag].run_time = 
      hpx_time_add(_profile_log.events[event].entries[*tag].run_time, dur);

  // then store counter information if necessary
  if (!_profile_log.events[event].simple) {
    // I leave this as type long long instead of int64_t to suppress a warning
    // at compile time that appears if I do otherwise
    long long values[_profile_log.num_counters];
    for (int i = 0; i < _profile_log.num_counters; i++) {
      values[i] = -1;
    }
    int retval = PAPI_stop(_profile_log.events[event].eventset, values);
    PAPI_reset(_profile_log.events[event].eventset);
    if (retval != PAPI_OK) {
      return retval;
    }
    
    for (int i = 0; i < _profile_log.num_counters; i++) {
      _profile_log.events[event].entries[*tag].counter_totals[i] 
        += (int64_t) values[i];
    }
  }
  _profile_log.events[event].entries[*tag].paused = true;
  return PAPI_OK;
}

int prof_resume(char *key, int *tag) {
  int event = profile_get_event(key);
  if (event < 0) {
    return PAPI_EINVAL;
  }

  if (*tag == HPX_PROF_NO_TAG) {
    for (int i = _profile_log.events[event].num_entries - 1; i >= 0; i--) {
      if (!_profile_log.events[event].entries[i].marked && 
         _profile_log.events[event].entries[i].paused) {
        *tag = i;
        break;
      }
    }
  }
  if (*tag == HPX_PROF_NO_TAG) {
    return PAPI_EINVAL;
  }

  _profile_log.events[event].entries[*tag].paused = false;
  _profile_log.events[event].entries[*tag].start_time = hpx_time_now();
  if (!_profile_log.events[event].simple) {
    PAPI_reset(_profile_log.events[_profile_log.current_event].eventset);
    return PAPI_start(_profile_log.events[event].eventset);
  }
  return PAPI_OK;
}
