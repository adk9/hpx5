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

#include "allreduce.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/padding.h"
#include "libhpx/Scheduler.h"
#include "libhpx/Worker.h"
#include <stdlib.h>
#include <atomic>

namespace {
constexpr auto ACQUIRE = std::memory_order_acquire;
constexpr auto RELEASE = std::memory_order_release;
constexpr auto RELAXED = std::memory_order_relaxed;
constexpr auto ACQ_REL = std::memory_order_acq_rel;
}

struct reduce {
  std::atomic<int> i;
  int n;
  size_t bytes;
  size_t padded;
  hpx_monoid_id_t id;
  hpx_monoid_op_t op;
  PAD_TO_CACHELINE(2 * sizeof(int) +
                   2 * sizeof(size_t) +
                   2 * sizeof(void(*)()));
  char values[];
};

reduce_t *reduce_new(size_t bytes, hpx_monoid_id_t id, hpx_monoid_op_t op) {
  reduce_t *r = NULL;

  // allocate enough space so that each
  int workers = here->sched->getNWorkers();
  size_t padded = _BYTES(HPX_CACHELINE_SIZE, bytes) + bytes;
  size_t size = sizeof(*r) + padded * workers;
  if (posix_memalign((void**)&r, HPX_CACHELINE_SIZE, size)) {
    dbg_error("could not allocate aligned reduction\n");
  }

  r->n = 0;
  r->bytes = bytes;
  r->padded = padded;
  r->id = id;
  r->op = op;
  r->i = 0;
  for (int i = 0, e = workers; i < e; ++i) {
    void *value = r->values + i * r->padded;
    r->id(value, r->bytes);
  }

  return r;
}

void reduce_delete(reduce_t *r) {
  if (r) {
    free(r);
  }
}

int reduce_add(reduce_t *r) {
  dbg_assert(r->i.load(ACQUIRE) == r->n);
  int i = ++r->n;
  r->i.store(i, RELEASE);
  return (i == 1);
}

int reduce_remove(reduce_t *r) {
  dbg_assert(r->i.load(ACQUIRE) == r->n);
  int i = --r->n;
  r->i.store(i, RELEASE);
  return (i == 0);
}

int reduce_join(reduce_t *r, const void *in) {
  void *buffer = r->values + libhpx::self->getId() * r->padded;
  r->op(buffer, in, r->bytes);
  return (r->i.fetch_sub(1, ACQ_REL) == 1);
}

void reduce_reset(reduce_t *r, void *out) {
  r->id(out, r->bytes);
  for (int i = 0, e = here->sched->getNWorkers(); i < e; ++i) {
    void *value = r->values + i * r->padded;
    r->op(out, value, r->bytes);
    r->id(value, r->bytes);
  }

  r->i.store(r->n, RELEASE);
}
