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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/padding.h>
#include <libhpx/scheduler.h>
#include <libhpx/topology.h>
#include "allreduce.h"

typedef struct {
  volatile int i;
  PAD_TO_CACHELINE(sizeof(int));
} count_t;

struct reduce {
  int              n;
  const int  padding;
  size_t       bytes;
  size_t      padded;
  hpx_monoid_id_t id;
  hpx_monoid_op_t op;
  count_t    *counts;
  char       *values;
  PAD_TO_CACHELINE(2 * sizeof(int) +
                   2 * sizeof(size_t) +
                   2 * sizeof(void(*)()) +
                   2 * sizeof(void*));
  char data[];
};

reduce_t *reduce_new(size_t bytes, hpx_monoid_id_t id, hpx_monoid_op_t op) {
  reduce_t *r = NULL;

  // allocate enough space so that each
  int workers = here->sched->n_workers;
  size_t padded = _BYTES(HPX_CACHELINE_SIZE, bytes) + bytes;
  size_t values = padded * workers;
  size_t counters = here->topology->nnodes * sizeof(count_t);
  size_t size = sizeof(*r) + counters + values;
  if (posix_memalign((void*)&r, HPX_CACHELINE_SIZE, size)) {
    dbg_error("could not allocate aligned reduction\n");
  }

  r->n = 0;
  r->bytes = bytes;
  r->padded = padded;
  r->id = id;
  r->op = op;
  r->counts = (void*)r->data;
  memset(r->counts, 0, counters);

  r->values = r->data + counters;
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
  int i = ++r->n;
  count_t *c = &r->counts[self->numa_node];
  sync_fadd(&c->i, 1, SYNC_ACQ_REL);
  return (i == 1);
}

int reduce_remove(reduce_t *r) {
  int i = --r->n;
  count_t *c = &r->counts[self->numa_node];
  sync_fadd(&c->i, -1, SYNC_ACQ_REL);
  return (i == 0);
}

/// Try and steal from a victim count.
///
/// This tries to bump the local count by 1/2 the victim's count.
///
/// @param        local The local count.
/// @param       victim The victim count.
///
/// @returns            The total count remaining.
static int _steal(volatile int *local, volatile int *victim) {
  int n;
  // figure out if there is a positive integer to steal.
  while ((n = sync_load(victim, SYNC_ACQUIRE)) > 0) {
    // We want to steal half of the available count. We use the ceiling because
    // if there is only one we want to steal the one.
    int steal = ceil_div_32(n, UINT32_C(2));

    // Need to make sure that we don't temporarily lose any count. This will
    // transiently increase the total count, but that's okay because we will
    // resolve the extra later. It's just important that we don't see any
    // false-positive zeros. Remember how many we have because we will return
    // that value to the caller so that they can figure out if the aggregate
    // value is 0.
    int have = sync_fadd(local, steal, SYNC_ACQ_REL) + steal;

    // Now go back and perform the steal operation. If this succeeds then
    // everything is good, and we can return the victim's count.
    int leave = n - steal;
    if (sync_cas(victim, n, leave, SYNC_ACQ_REL, SYNC_RELAXED)) {
      log_coll("stole %d, left %d, have %d\n", steal, leave, have);
      return leave + have;
    }

    // Oops, someone else got some of the victim's count. Reset our local count
    // and retry the operation. All sorts of craziness might have happened
    // locally while we had "extra" local count, but that doesn't matter to us,
    // because no one will have inferred the global count as 0.
    sync_fadd(local, -steal, SYNC_ACQ_REL);
  }
  return n + sync_load(local, SYNC_ACQUIRE);
}

/// Try and balance the counter.
///
/// Once a numa node's local count is depleted, we need to figure out if the
/// entire count is depleted, and we'd like to balance the counts if the total
/// is still non-zero. This will return 0 if the total count is 0.
///
/// @param       counts The count array.
/// @param        local The index of the local numa node.
///
/// @returns          0 If the total count is zero.
///                 > 0 Otherwise.
static int _balance(count_t *counts, int local) {
  log_coll("balancing reduce from numa node %d\n", local);
  for (int i = 1, nodes = here->topology->nnodes; i < nodes; ++i) {
    int victim = (local + i) % nodes;
    int count = _steal(&counts[local].i, &counts[victim].i);
    if (count) {
      return count;
    }
  }
  int i = sync_load(&counts[local].i, SYNC_ACQUIRE);
  log_coll("detected count==i from numa node %d\n", local);
  return i;
}

int reduce_join(reduce_t *r, const void *in) {
  void *buffer = r->values + self->id * r->padded;
  r->op(buffer, in, r->bytes);
  int numa = self->numa_node;
  count_t *c = &r->counts[numa];
  if (sync_fadd(&c->i, -1, SYNC_ACQ_REL) > 1) {
    return 0;
  }
  else {
    return (_balance(r->counts, numa) == 0);
  }
}

void reduce_reset(reduce_t *r, void *out) {
  r->id(out, r->bytes);
  for (int i = 0, e = here->sched->n_workers; i < e; ++i) {
    void *value = r->values + i * r->padded;
    r->op(out, value, r->bytes);
    r->id(value, r->bytes);
  }

  // have to reset everyone's counts
  int nodes = here->topology->nnodes;
  int n = r->n / nodes;
  int m = r->n % nodes;
  log_coll("resetting %p to %d\n", (void*)r, r->n);
  for (int i = 0, e = nodes; i < e; ++i) {
    int v = n + ((i < m) ? 1 : 0);
    log_coll("adding %d to %p at node %d\n", v, (void*)r, i);
    sync_store(&r->counts[i].i, v, SYNC_RELEASE);
  }
}
