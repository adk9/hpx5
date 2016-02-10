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

#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/instrumentation.h>
#include <libhpx/parcel.h>
#include <libhpx/worker.h>

/// Instrumentation events.
#define  TRACE_EVENT_PARCEL_CREATE INT32_C(0)
#define    TRACE_EVENT_PARCEL_SEND INT32_C(1)
#define    TRACE_EVENT_PARCEL_RECV INT32_C(2)
#define     TRACE_EVENT_PARCEL_RUN INT32_C(3)
#define     TRACE_EVENT_PARCEL_END INT32_C(4)
#define TRACE_EVENT_PARCEL_SUSPEND INT32_C(5)
#define  TRACE_EVENT_PARCEL_RESUME INT32_C(6)
#define  TRACE_EVENT_PARCEL_RESEND INT32_C(7)

#define TRACE_EVENT_NETWORK_PWC_SEND INT32_C(8)
#define TRACE_EVENT_NETWORK_PWC_RECV INT32_C(9)

#define     TRACE_EVENT_SCHED_WQSIZE INT32_C(10)
#define  TRACE_EVENT_SCHED_PUSH_LIFO INT32_C(11)
#define   TRACE_EVENT_SCHED_POP_LIFO INT32_C(12)
#define TRACE_EVENT_SCHED_STEAL_LIFO INT32_C(13)
#define      TRACE_EVENT_SCHED_ENTER INT32_C(14)
#define       TRACE_EVENT_SCHED_EXIT INT32_C(15)

#define          TRACE_EVENT_LCO_INIT INT32_C(16)
#define        TRACE_EVENT_LCO_DELETE INT32_C(17)
#define           TRACE_EVENT_LCO_SET INT32_C(18)
#define         TRACE_EVENT_LCO_RESET INT32_C(19)
#define TRACE_EVENT_LCO_ATTACH_PARCEL INT32_C(20)
#define          TRACE_EVENT_LCO_WAIT INT32_C(21)
#define       TRACE_EVENT_LCO_TRIGGER INT32_C(22)

#define    TRACE_EVENT_PROCESS_NEW INT32_C(23)
#define   TRACE_EVENT_PROCESS_CALL INT32_C(24)
#define TRACE_EVENT_PROCESS_DELETE INT32_C(25)

#define TRACE_EVENT_MEMORY_REGISTERED_ALLOC INT32_C(26)
#define  TRACE_EVENT_MEMORY_REGISTERED_FREE INT32_C(27)
#define     TRACE_EVENT_MEMORY_GLOBAL_ALLOC INT32_C(28)
#define      TRACE_EVENT_MEMORY_GLOBAL_FREE INT32_C(29)
#define     TRACE_EVENT_MEMORY_CYCLIC_ALLOC INT32_C(30)
#define      TRACE_EVENT_MEMORY_CYCLIC_FREE INT32_C(31)
#define TRACE_EVENT_MEMORY_ENTER_ALLOC_FREE INT32_C(32)

#define        TRACE_EVENT_SCHEDTIMES_SCHED INT32_C(33)
#define        TRACE_EVENT_SCHEDTIMES_PROBE INT32_C(34)
#define     TRACE_EVENT_SCHEDTIMES_PROGRESS INT32_C(35)

#define                 TRACE_EVENT_BOOKEND INT32_C(36)
#define                    TRACE_NUM_EVENTS INT32_C(37)

static const char * const TRACE_EVENT_TO_STRING[] = {
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

  "BOOKEND"
};

static const int TRACE_OFFSETS[] = {
  TRACE_EVENT_PARCEL_CREATE,
  TRACE_EVENT_NETWORK_PWC_SEND,
  TRACE_EVENT_SCHED_WQSIZE,
  TRACE_EVENT_LCO_INIT,
  TRACE_EVENT_PROCESS_NEW,
  TRACE_EVENT_MEMORY_REGISTERED_ALLOC,
  TRACE_EVENT_SCHEDTIMES_SCHED,
  TRACE_EVENT_BOOKEND,
  TRACE_NUM_EVENTS
};

/// Scheduler tracing events.
/// @{
static inline void EVENT_SCHED_WQSIZE(worker_t *w) {
  inst_trace(TRACE_SCHED, TRACE_EVENT_SCHED_WQSIZE,
             sync_chase_lev_ws_deque_size(&w->queues[w->work_id].work));
}

static inline void EVENT_SCHED_PUSH_LIFO(hpx_parcel_t *p) {
  inst_trace(TRACE_SCHED, TRACE_EVENT_SCHED_PUSH_LIFO, p);
}

static inline void EVENT_SCHED_POP_LIFO(hpx_parcel_t *p) {
  inst_trace(TRACE_SCHED, TRACE_EVENT_SCHED_POP_LIFO, p);
}

static inline void EVENT_SCHED_STEAL_LIFO(hpx_parcel_t *p, const worker_t *victim) {
  inst_trace(TRACE_SCHED, TRACE_EVENT_SCHED_STEAL_LIFO, p, victim->id);
}

static inline void EVENT_SCHED_ENTER() {
  inst_trace(TRACE_SCHED, TRACE_EVENT_SCHED_ENTER);
}

static inline void EVENT_SCHED_EXIT() {
  inst_trace(TRACE_SCHED, TRACE_EVENT_SCHED_EXIT);
}

static inline void EVENT_THREAD_RUN(hpx_parcel_t *p, worker_t *w) {
#ifdef HAVE_APEX
  // if this is NOT a null or lightweight action, send a "start" event to APEX
  if (p->action != hpx_lco_set_action) {
    CHECK_ACTION(p->action);
    void* handler = (void*)actions[p->action].handler;
    w->profiler = (void*)(apex_start(APEX_FUNCTION_ADDRESS, handler));
  }
#endif
  inst_trace(TRACE_PARCEL, TRACE_EVENT_PARCEL_RUN, p->id, p->action, p->size);
}

static inline void EVENT_THREAD_END(hpx_parcel_t *p, worker_t *w) {
#ifdef HAVE_APEX
  if (w->profiler != NULL) {
    apex_stop((apex_profiler_handle)(w->profiler));
    w->profiler = NULL;
  }
#endif
  inst_trace(TRACE_PARCEL, TRACE_EVENT_PARCEL_END, p->id, p->action);
}

static inline void EVENT_THREAD_SUSPEND(hpx_parcel_t *p, worker_t *w) {
#ifdef HAVE_APEX
  if (w->profiler != NULL) {
    apex_stop((apex_profiler_handle)(w->profiler));
    w->profiler = NULL;
  }
#endif
  inst_trace(TRACE_PARCEL, TRACE_EVENT_PARCEL_SUSPEND, p->id, p->action);
}

static inline void EVENT_THREAD_RESUME(hpx_parcel_t *p, worker_t *w) {
#ifdef HAVE_APEX
  if (p->action != hpx_lco_set_action) {
    void* handler = (void*)actions[p->action].handler;
    w->profiler = (void*)(apex_resume(APEX_FUNCTION_ADDRESS, handler));
  }
#endif
  inst_trace(TRACE_PARCEL, TRACE_EVENT_PARCEL_RESUME, p->id, p->action);
}
/// @}


/// Process tracing events.
/// @{
static inline void EVENT_PROCESS_NEW(hpx_addr_t process, hpx_addr_t termination) {
  inst_trace(TRACE_PROCESS, TRACE_EVENT_PROCESS_NEW, process, termination);
}

static inline void EVENT_PROCESS_CALL(hpx_addr_t process, hpx_addr_t pid) {
  inst_trace(TRACE_PROCESS, TRACE_EVENT_PROCESS_CALL, process, pid);
}

static inline void EVENT_PROCESS_DELETE(hpx_addr_t process) {
  inst_trace(TRACE_PROCESS, TRACE_EVENT_PROCESS_DELETE, process);
}
/// @}


/// Memory tracing events.
/// @{
static inline void EVENT_MEMORY_REGISTERED_MALLOC(void *ptr, size_t n,
                                                  size_t align) {
  inst_trace(TRACE_MEMORY, TRACE_EVENT_MEMORY_REGISTERED_ALLOC, ptr, n, align);
}

static inline void EVENT_MEMORY_REGISTERED_FREE(void *ptr) {
  inst_trace(TRACE_MEMORY, TRACE_EVENT_MEMORY_REGISTERED_FREE, ptr);
}

static inline void EVENT_MEMORY_GLOBAL_MALLOC(void *ptr, size_t n,
                                              size_t align) {
  inst_trace(TRACE_MEMORY, TRACE_EVENT_MEMORY_GLOBAL_ALLOC, ptr, n, align);
}

static inline void EVENT_MEMORY_GLOBAL_FREE(void *ptr) {
  inst_trace(TRACE_MEMORY, TRACE_EVENT_MEMORY_GLOBAL_FREE, ptr);
}

static inline void EVENT_MEMORY_CYCLIC_MALLOC(void *ptr, size_t n,
                                              size_t align) {
  inst_trace(TRACE_MEMORY, TRACE_EVENT_MEMORY_CYCLIC_ALLOC, ptr, n, align);
}

static inline void EVENT_MEMORY_CYCLIC_FREE(void *ptr) {
  inst_trace(TRACE_MEMORY, TRACE_EVENT_MEMORY_CYCLIC_FREE, ptr);
}

static inline void EVENT_MEMORY_ENTER_ALLOC_FREE() {
  inst_trace(TRACE_MEMORY, TRACE_EVENT_MEMORY_ENTER_ALLOC_FREE);
}
/// @}


/// Parcel tracing events.
/// @{
static inline void EVENT_PARCEL_CREATE(hpx_parcel_t *p, hpx_parcel_t *parent) {
  inst_trace(TRACE_PARCEL, TRACE_EVENT_PARCEL_CREATE, p->id, p->action,
             p->size, ((parent) ? parent->id : 0));
}

static inline void EVENT_PARCEL_SEND(hpx_parcel_t *p) {
  inst_trace(TRACE_PARCEL, TRACE_EVENT_PARCEL_SEND, p->id, p->action,
             p->size, p->target);
}

static inline void EVENT_PARCEL_RECV(hpx_parcel_t *p) {
  inst_trace(TRACE_PARCEL, TRACE_EVENT_PARCEL_RECV, p->id, p->action,
             p->size, p->src);
}

static inline void EVENT_PARCEL_RESEND(hpx_parcel_t *p) {
  inst_trace(TRACE_PARCEL, TRACE_EVENT_PARCEL_RESEND, p->id, p->action,
             p->size, p->target);
}
/// @}

static inline void EVENT_SCHEDTIMES_PROGRESS(uint64_t time) {
  inst_trace(TRACE_SCHEDTIMES, TRACE_EVENT_SCHEDTIMES_PROGRESS, time);
}

static inline void EVENT_SCHEDTIMES_PROBE(uint64_t time) {
  inst_trace(TRACE_SCHEDTIMES, TRACE_EVENT_SCHEDTIMES_PROBE, time);
}


#ifdef __cplusplus
}
#endif

#endif // LIBHPX_EVENTS_H
