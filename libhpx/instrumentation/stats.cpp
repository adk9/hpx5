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

#include <fcntl.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <hpx/hpx.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/Scheduler.h>
#include "libhpx/Worker.h"
#include "metadata.h"

namespace {
using libhpx::Worker;
}

static void _vappend(int UNUSED, int n, int id, ...) {
  libhpx::self->stats[id]++;
}

static void _start(void) {
  for (auto&& w : here->sched->getWorkers()) {
    w->stats = static_cast<uint64_t*>(calloc(TRACE_NUM_EVENTS, sizeof(uint64_t)));
  }
}

static void _destroy(void) {
  Worker *master = here->sched->getWorker(0);
  for (int k = 1; k < HPX_THREADS; ++k) {
    Worker *w = here->sched->getWorker(k);
    for (unsigned i = 0; i < TRACE_NUM_EVENTS; ++i) {
      int c = TRACE_EVENT_TO_CLASS[i];
      if (inst_trace_class(c)) {
        master->stats[i] += w->stats[i];
      }
    }
    free(w->stats);
  }

  for (unsigned i = 0; i < TRACE_NUM_EVENTS; ++i) {
    int c = TRACE_EVENT_TO_CLASS[i];
    if (inst_trace_class(c)) {
             printf("%d,%s,%" PRIu64 "\n",
             here->rank,
             TRACE_EVENT_TO_STRING[i],
             master->stats[i]);
    }
  }
  free(master->stats);
}

trace_t *trace_stats_new(const config_t *cfg) {
  trace_t *trace = static_cast<trace_t*>(malloc(sizeof(*trace)));
  dbg_assert(trace);

  trace->type        = HPX_TRACE_BACKEND_STATS;
  trace->start       = _start;
  trace->destroy     = _destroy;
  trace->vappend     = _vappend;
  sync_store(&trace->active, !cfg->trace_off, SYNC_RELAXED);
  return trace;
}
