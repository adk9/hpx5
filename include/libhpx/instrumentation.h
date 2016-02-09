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
int inst_start();

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

// This matches the order in config.h trace_t.
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

#define  INST_EVENT_PARCEL_CREATE INT32_C(0)
#define    INST_EVENT_PARCEL_SEND INT32_C(1)
#define    INST_EVENT_PARCEL_RECV INT32_C(2)
#define     INST_EVENT_PARCEL_RUN INT32_C(3)
#define     INST_EVENT_PARCEL_END INT32_C(4)
#define INST_EVENT_PARCEL_SUSPEND INT32_C(5)
#define  INST_EVENT_PARCEL_RESUME INT32_C(6)
#define  INST_EVENT_PARCEL_RESEND INT32_C(7)

#define INST_EVENT_NETWORK_PWC_SEND INT32_C(8)
#define INST_EVENT_NETWORK_PWC_RECV INT32_C(9)

#define     INST_EVENT_SCHED_WQSIZE INT32_C(10)
#define  INST_EVENT_SCHED_PUSH_LIFO INT32_C(11)
#define   INST_EVENT_SCHED_POP_LIFO INT32_C(12)
#define INST_EVENT_SCHED_STEAL_LIFO INT32_C(13)
#define      INST_EVENT_SCHED_ENTER INT32_C(14)
#define       INST_EVENT_SCHED_EXIT INT32_C(15)

#define          INST_EVENT_LCO_INIT INT32_C(16)
#define        INST_EVENT_LCO_DELETE INT32_C(17)
#define           INST_EVENT_LCO_SET INT32_C(18)
#define         INST_EVENT_LCO_RESET INT32_C(19)
#define INST_EVENT_LCO_ATTACH_PARCEL INT32_C(20)
#define          INST_EVENT_LCO_WAIT INT32_C(21)
#define       INST_EVENT_LCO_TRIGGER INT32_C(22)

#define    INST_EVENT_PROCESS_NEW INT32_C(23)
#define   INST_EVENT_PROCESS_CALL INT32_C(24)
#define INST_EVENT_PROCESS_DELETE INT32_C(25)

#define INST_EVENT_MEMORY_REGISTERED_ALLOC INT32_C(26)
#define  INST_EVENT_MEMORY_REGISTERED_FREE INT32_C(27)
#define     INST_EVENT_MEMORY_GLOBAL_ALLOC INT32_C(28)
#define      INST_EVENT_MEMORY_GLOBAL_FREE INT32_C(29)
#define     INST_EVENT_MEMORY_CYCLIC_ALLOC INT32_C(30)
#define      INST_EVENT_MEMORY_CYCLIC_FREE INT32_C(31)
#define INST_EVENT_MEMORY_ENTER_ALLOC_FREE INT32_C(32)

#define        INST_EVENT_SCHEDTIMES_SCHED INT32_C(33)
#define        INST_EVENT_SCHEDTIMES_PROBE INT32_C(34)
#define     INST_EVENT_SCHEDTIMES_PROGRESS INT32_C(35)

#define                 INST_EVENT_BOOKEND INT32_C(36)
#define                    INST_NUM_EVENTS INT32_C(37)

static const char * const INST_EVENT_TO_STRING[] = {
  "PARCEL_CREATE",
  "PARCEL_SEND",
  "PARCEL_RECV",
  "PARCEL_RUN",
  "PARCEL_END",
  "PARCEL_SUSPEND",
  "PARCEL_RESUME",
  "PARCEL_RESEND",

  "NETWORK_PWC_SEND",
  "NETWORK_PWC_RECV",

  "SCHED_WQSIZE",
  "SCHED_PUSH_LIFO",
  "SCHED_POP_LIFO",
  "SCHED_STEAL_LIFO",
  "SCHED_ENTER",
  "SCHED_EXIT",

  "LCO_INIT",
  "LCO_DELETE",
  "LCO_SET",
  "LCO_RESET",
  "LCO_ATTACH_PARCEL",
  "LCO_WAIT",
  "LCO_TRIGGER",

  "PROCESS_NEW",
  "PROCESS_CALL",
  "PROCESS_DELETE",

  "MEMORY_REGISTERED_ALLOC",
  "MEMORY_REGISTERED_FREE",
  "MEMORY_GLOBAL_ALLOC",
  "MEMORY_GLOBAL_FREE",
  "MEMORY_CYCLIC_ALLOC",
  "MEMORY_CYCLIC_FREE",
  "MEMORY_ENTER_ALLOC_FREE",

  "SCHEDTIMES_SCHED",
  "SCHEDTIMES_PROBE",
  "SCHEDTIMES_PROGRESS",

  "INST_BOOKEND"
};

static const int INST_OFFSETS[] = {
  INST_EVENT_PARCEL_CREATE,
  INST_EVENT_NETWORK_PWC_SEND,
  INST_EVENT_SCHED_WQSIZE,
  INST_EVENT_LCO_INIT,
  INST_EVENT_PROCESS_NEW,
  INST_EVENT_MEMORY_REGISTERED_ALLOC,
  INST_EVENT_SCHEDTIMES_SCHED,
  INST_EVENT_BOOKEND,
  INST_NUM_EVENTS
};

static inline bool inst_trace_class(int type) {
  return config_trace_classes_isset(here->config, 1 << type);
}

#endif
