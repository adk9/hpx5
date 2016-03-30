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

#ifndef LIBHPX_SCHEDULER_EVENTS_H
#define LIBHPX_SCHEDULER_EVENTS_H

/// @file libhpx/scheduler/events.h
/// @short Define the set of scheduler events that we know how to trace.

#ifdef __cplusplus
extern "C" {
#endif

struct hpx_parcel;
struct worker;

#ifdef ENABLE_INSTRUMENTATION
void EVENT_THREAD_RUN(struct hpx_parcel *p, struct worker *w);
void EVENT_THREAD_END(struct hpx_parcel *p, struct worker *w);
void EVENT_THREAD_SUSPEND(struct hpx_parcel *p, struct worker *w);
void EVENT_THREAD_RESUME(struct hpx_parcel *p, struct worker *w);
#else
# define EVENT_THREAD_RUN(...)
# define EVENT_THREAD_END(...)
# define EVENT_THREAD_SUSPEND(...)
# define EVENT_THREAD_RESUME(...)
#endif

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_SCHEDULER_EVENTS_H
