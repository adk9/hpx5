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
#include <stdlib.h>
#include <hpx/attributes.h>
#include <hpx/builtins.h>
#include <libhpx/config.h>
#include <libhpx/locality.h>

/// INST will do @p stmt only if instrumentation is enabled
#ifdef ENABLE_INSTRUMENTATION
# define INST(stmt) stmt;
# define INST_IF(S) if (S)
#else
# define INST(stmt)
# define INST_IF(S) if (false)
#endif

typedef struct trace {
  int type;
  volatile bool active;
  void (*start)(void);
  void (*destroy)(void);
  void (*vappend)(int type, int n, int id, ...);
} trace_t;

/// Initialize tracing. This is usually called in hpx_init().
trace_t *trace_new(const config_t *cfg)
  HPX_NON_NULL(1) HPX_MALLOC;

/// "Start" tracing. This is usually called in
/// hpx_run(). This takes
/// care of some things that must be done after initialization is
/// complete,
/// specifically action registration.
static inline
void trace_start(void *obj) {
  if (obj) {
    const trace_t *t = (trace_t*)obj;
    t->start();
  }
}
  
/// Delete a trace object.
///
/// @param      obj The trace object to delete.
static inline void
trace_destroy(void *obj) {
  if (obj) {
    trace_t *t = (trace_t*)obj;
    t->destroy();
  }
}

/// Record an event to the log
///
/// @param        type Type this event is part of (see hpx_inst_class_type_t)
/// @param            n The number of user arguments to log, < 5.
/// @param           id The event id (see hpx_inst_event_type_t)
#ifdef ENABLE_INSTRUMENTATION
# define trace_append(type, ...)                                 \
  if (here->tracer) {                                            \
    here->tracer->vappend(type, __HPX_NARGS(__VA_ARGS__) - 1,    \
                          __VA_ARGS__); }
#else
# define trace_append(type, id, ...)
#endif

static inline bool inst_trace_class(int type) {
  return config_trace_classes_isset(here->config, type);
}

#ifdef __cplusplus
}
#endif

// Constructor for a "file" tracer object.
trace_t *trace_file_new(const config_t *cfg);

// Constructor for a "console" tracer object.
trace_t *trace_console_new(const config_t *cfg);

// Constructor for the "stats" tracer object.
trace_t *trace_stats_new(const config_t *cfg);

#endif
