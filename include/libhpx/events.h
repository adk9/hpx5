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

#ifndef LIBHPX_EVENTS_H
#define LIBHPX_EVENTS_H

/// @file include/libhpx/events.h
/// @short Define the set of instrumentation events that we know how to trace.

#ifdef __cplusplus
extern "C" {
#endif

#include <hpx/builtins.h>
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/instrumentation.h>
#include <libhpx/parcel.h>
#include <libhpx/worker.h>

/// Tracing events.
///
/// This generates an enumerated type that lists the all of the
/// available events for tracing.
typedef enum {
#define LIBHPX_EVENT(class, event, ...) TRACE_EVENT_##class##_##event,
# include "events.def"
#undef LIBHPX_EVENT
} libhpx_trace_events_t;

/// Event names
///
/// This generates a table that maps tracing events to their names.
static const char *const TRACE_EVENT_TO_STRING[] = {
#define LIBHPX_EVENT(class, event, ...) _HPX_XSTR(class##_##event),
# include "events.def"
#undef LIBHPX_EVENT
};

/// Number of fields per event.
///
/// When outputting trace files, we need to know the number of fields
/// in each trace event's payload. This table maintains that
/// information by counting the number of arguments to the event
/// definition.
static const int TRACE_EVENT_NUM_FIELDS[] = {
#define LIBHPX_EVENT(class, event, ...) __HPX_NARGS(__VA_ARGS__),
# include "events.def"
#undef LIBHPX_EVENT
};

/// Total number of trace events.
#define TRACE_NUM_EVENTS _HPX_NELEM(TRACE_EVENT_TO_STRING)

/// Offsets into the trace table indicating the beginning of each
/// trace class.
static const int TRACE_OFFSETS[] = {
  TRACE_EVENT_PARCEL_CREATE,
  TRACE_EVENT_NETWORK_SEND,
  TRACE_EVENT_SCHED_WQSIZE,
  TRACE_EVENT_LCO_INIT,
  TRACE_EVENT_PROCESS_NEW,
  TRACE_EVENT_MEMORY_ALLOC_BEGIN,
  TRACE_EVENT_TRACE_FILE_IO_BEGIN,
  TRACE_EVENT_GAS_ACCESS,
  TRACE_EVENT_COLLECTIVE_NEW,
  TRACE_NUM_EVENTS
};

/// Trace event macros.
///
/// The trace event macros are generated automatically from their
/// definitions in the events.def file. When instrumentation is
/// disabled, we use empty stubs declared in event_stubs.h that is
/// generated at configure-time (since the C preprocessor disallows
/// generating macros from other macros). Otherwise, we generate the
/// event macros from the event definitions using the helper macros
/// _DECL and _ARGS. These macros take the variadic type arguments to
/// an event, and uses identifiers to instantiate the types.
#ifndef ENABLE_INSTRUMENTATION
# include <libhpx/event_stubs.h>
#else
# define _DECL0() void
# define _DECL1(t0) t0 u0
# define _DECL2(t0,t1) t0 u0, t1 u1
# define _DECL3(t0,t1,t2) t0 u0, t1 u1, t2 u2
# define _DECL4(t0,t1,t2,t3) t0 u0, t1 u1, t2 u2, t3 u3
# define _DECL5(t0,t1,t2,t3,t4) t0 u0, t1 u1, t2 u2, t3 u3, t4 u4
# define _ARGS0
# define _ARGS1 , u0
# define _ARGS2 , u0, u1
# define _ARGS3 , u0, u1, u2
# define _ARGS4 , u0, u1, u2, u3
# define _ARGS5 , u0, u1, u2, u3, u4
# define LIBHPX_EVENT(class, event, ...)                                \
  static inline void                                                    \
  EVENT_##class##_##event(_HPX_CAT2(_DECL, __HPX_NARGS(__VA_ARGS__))(__VA_ARGS__)) { \
    trace_append(HPX_TRACE_##class, TRACE_EVENT_##class##_##event         \
              _HPX_CAT2(_ARGS, __HPX_NARGS(__VA_ARGS__)));              \
  }
# include "events.def"
# undef LIBHPX_EVENT
# undef _ARGS0
# undef _ARGS1
# undef _ARGS2
# undef _ARGS3
# undef _ARGS4
# undef _ARGS5
# undef _DECL0
# undef _DECL1
# undef _DECL2
# undef _DECL3
# undef _DECL4
# undef _DECL5
#endif

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_EVENTS_H
