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

#include <hpx/hpx.h>
#include <libhpx/events.h>
#include <libhpx/worker.h>
#include "events.h"

/// Thread tracing events.
/// @{
void EVENT_THREAD_RUN(hpx_parcel_t *p, worker_t *w) {
  if (p == w->system) {
    return;
  }
#ifdef HAVE_APEX
  // if this is NOT a null or lightweight action, send a "start" event to APEX
  if (p->action != hpx_lco_set_action) {
    CHECK_ACTION(p->action);
    void* handler = (void*)actions[p->action].handler;
    w->profiler = (void*)(apex_start(APEX_FUNCTION_ADDRESS, handler));
  }
#endif
  EVENT_PARCEL_RUN(p->id, p->action, p->size);
}

void EVENT_THREAD_END(hpx_parcel_t *p, worker_t *w) {
  if (p == w->system) {
    return;
  }
#ifdef HAVE_APEX
  if (w->profiler != NULL) {
    apex_stop((apex_profiler_handle)(w->profiler));
    w->profiler = NULL;
  }
#endif
  EVENT_PARCEL_END(p->id, p->action);
}

void EVENT_THREAD_SUSPEND(hpx_parcel_t *p, worker_t *w) {
  if (p == w->system) {
    return;
  }
#ifdef HAVE_APEX
  if (w->profiler != NULL) {
    apex_stop((apex_profiler_handle)(w->profiler));
    w->profiler = NULL;
  }
#endif
  EVENT_PARCEL_SUSPEND(p->id, p->action);
}

void EVENT_THREAD_RESUME(hpx_parcel_t *p, worker_t *w) {
  if (p == w->system) {
    return;
  }
#ifdef HAVE_APEX
  if (p->action != hpx_lco_set_action) {
    void* handler = (void*)actions[p->action].handler;
    w->profiler = (void*)(apex_resume(APEX_FUNCTION_ADDRESS, handler));
  }
#endif
  EVENT_PARCEL_RESUME(p->id, p->action);
}
/// @}

