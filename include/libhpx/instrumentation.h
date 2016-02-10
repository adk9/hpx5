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

#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <hpx/attributes.h>
#include <hpx/builtins.h>
#include <libhpx/config.h>
#include <libhpx/locality.h>
#include <libhpx/profiling.h>

struct config;

/// INST will do @p stmt only if instrumentation is enabled
#ifdef ENABLE_INSTRUMENTATION
# define INST(stmt) stmt;
#else
# define INST(stmt)
#endif

/// Initialize instrumentation. This is usually called in hpx_init().
int inst_init(struct config *cfg)
  HPX_NON_NULL(1);

/// "Start" instrumentation. This is usually called in hpx_run(). This takes
/// care of some things that must be done after initialization is complete,
/// specifically action registration.
int inst_start(void);

/// Finalize the instrumentation framework.
void inst_fini(void);

/// Dump all of the profiling information to file
void inst_prof_dump(profile_log_t profile_log);

/// Record an event to the log
/// @param        type Type this event is part of (see hpx_inst_class_type_t)
/// @param           id The event id (see hpx_inst_event_type_t)
/// @param            n The number of user arguments to log, < 5.
void inst_vtrace(int type, int n, int id, ...);

#ifdef ENABLE_INSTRUMENTATION
# define inst_trace(type, ...)                                 \
  inst_vtrace(type, __HPX_NARGS(__VA_ARGS__) - 1, __VA_ARGS__)
#else
# define inst_trace(type, id, ...)
#endif

/// Instrumentation classes.
/// This matches the order in config.h trace_t.
#define      INST_PARCEL INT32_C(0)
#define INST_NETWORK_PWC INT32_C(1)
#define       INST_SCHED INT32_C(2)
#define         INST_LCO INT32_C(3)
#define     INST_PROCESS INT32_C(4)
#define      INST_MEMORY INT32_C(5)
#define  INST_SCHEDTIMES INT32_C(6)
#define     INST_BOOKEND INT32_C(7)
#define INST_NUM_CLASSES INT32_C(8)

static const char * const INST_CLASS_TO_STRING[] = {
  "PARCEL",
  "NETWORK_PWC",
  "SCHED",
  "LCO",
  "PROCESS",
  "MEMORY",
  "SCHEDTIMES",
  "BOOKEND"
};

static inline bool inst_trace_class(int type) {
  return config_trace_classes_isset(here->config, 1 << type);
}

#ifdef __cplusplus
}
#endif

#endif
