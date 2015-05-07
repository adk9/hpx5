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
#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

#include <stdint.h>
#include <hpx/attributes.h>
#include <hpx/builtins.h>
#include <libhpx/locality.h> // for here object

struct config;

int inst_init(struct config *cfg)
  HPX_INTERNAL HPX_NON_NULL(1);

void inst_fini(void)
  HPX_INTERNAL;

/// Record an event to the log
/// @param        class Class this event is part of (see hpx_inst_class_type_t)
/// @param           id The event id (see hpx_inst_event_type_t)
/// @param            n The number of user arguments to log, < 5.
/// @param      va_args The user arguments.
void inst_vtrace(int class, int n, int id, ...)
  HPX_INTERNAL;

#ifdef ENABLE_INSTRUMENTATION
# define inst_trace(class, ...)                                 \
  inst_vtrace(class, __HPX_NARGS(__VA_ARGS__) - 1, __VA_ARGS__)
#else
# define inst_trace(class, id, ...)             \
  do {                                          \
    (void)class;                                \
    (void)id;                                   \
  } while (0)
#endif

#define _INST_CLASS_PARCEL 0
#define _INST_CLASS_NETWORK_PWC 1
#define _INST_SCHED 2

// This matches the order in config.h trace_t.
typedef enum {
  HPX_INST_CLASS_PARCEL = _INST_CLASS_PARCEL,
  HPX_INST_CLASS_NETWORK_PWC,
  INST_SCHED,

  HPX_INST_NUM_CLASSES
} hpx_inst_class_type_t;

static const char * const INST_CLASS_TO_STRING[] = {
  "CLASS_PARCEL",
  "CLASS_NETWORK_PWC",
  "CLASS_SCHED"
};

#define _INST_EVENT_PARCEL_CREATE 0
#define _INST_EVENT_PARCEL_SEND 1
#define _INST_EVENT_PARCEL_RECV 2
#define _INST_EVENT_PARCEL_RUN 3
#define _INST_EVENT_PARCEL_END 4
#define _INST_EVENT_NETWORK_PWC_SEND 5
#define _INST_EVENT_NETWORK_PWC_RECV 6
#define _INST_SCHED_WQSIZE 7

typedef enum {
  HPX_INST_EVENT_PARCEL_CREATE = _INST_EVENT_PARCEL_CREATE,
  HPX_INST_EVENT_PARCEL_SEND,
  HPX_INST_EVENT_PARCEL_RECV,
  HPX_INST_EVENT_PARCEL_RUN,
  HPX_INST_EVENT_PARCEL_END,

  HPX_INST_EVENT_NETWORK_PWC_SEND,
  HPX_INST_EVENT_NETWORK_PWC_RECV,

  INST_SCHED_WQSIZE,

  HPX_INST_NUM_EVENTS
} hpx_inst_event_type_t;

static const char * const INST_EVENT_TO_STRING[] = {
  "EVENT_PARCEL_CREATE",
  "EVENT_PARCEL_SEND",
  "EVENT_PARCEL_RECV",
  "EVENT_PARCEL_RUN",
  "EVENT_PARCEL_END",
  "EVENT_NETWORK_PWC_SEND",
  "EVENT_NETWORK_PWC_RECV",
  "WQSIZE"
};

static const int INST_OFFSETS[] = {
  HPX_INST_EVENT_PARCEL_CREATE,
  HPX_INST_EVENT_NETWORK_PWC_SEND,
  INST_SCHED_WQSIZE,
  HPX_INST_NUM_EVENTS
};

static inline bool inst_trace_class(int class) {
  return config_trace_classes_isset(here->config, 1 << class);
}

#endif
