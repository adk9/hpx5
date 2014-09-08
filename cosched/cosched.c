// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
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

#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>

#include "libsync/barriers.h"
#include "libsync/deques.h"
#include "libhpx/libhpx.h"
#include "cosched.h"

#define ATOMIC SYNC_ATOMIC()
#define FINC(addr) sync_fadd(addr, 1, SYNC_ACQ_REL)
#define LOAD(addr) sync_load_i32(addr, SYNC_ACQUIRE)

/// Each core worker thread has it's own scheduling data. This type represents
/// that structure.
///
/// @field      id The id for this scheduling block.
///
/// @field padding Separate the data that can be shared read-only from the
///                read-write data that is accessed concurrently, and make sure
///                internal alignments make sense.
//
/// @field    work The scheduler's work queue.
typedef struct {
  int id;
  const char padding[HPX_CACHELINE_SIZE - sizeof(int)];
  chase_lev_ws_deque_t work;
} worker_data_t HPX_ALIGNED(HPX_CACHELINE_SIZE);


/// There is one cooperative scheduler per PE (i.e., address space).
///
/// @field     max The maximum number of worker threads, constant after
///                initialization and used to size other structures.
///
/// @field barrier A barrier for the PE. This is initialized to expect @p max,
///                named workers to join. With proper synchronization, it could
///                be dynamically resized.
///
/// @field threads A @p max sized array of pthread ids, so that we can properly
///                join workers during shutdown.
///
/// @field workers A @p max sized array of pointers to worker data, so that
///                workers can find other workers' data as needed, e.g., for
///                work stealing. Workers will initialize their own entries in
///                this array.
static struct {
  size_t max;
  ATOMIC int next;
  ATOMIC int shutdown;
  barrier_t *barrier;
  pthread_t *threads;
  worker_data_t **workers;
} here = {
  .max = 0,
  .next = 0,
  .shutdown = 0,
  .barrier = NULL,
  .threads = NULL,
  .workers = NULL
};


/// Every pthread in the system is a worker thread, and has the associated
/// worker data. We use the __thread local extension for this structure as a
/// convenience, the alternative would be using a pthread_{set,get}specific
/// allocation to store the pointer to a worker data structure or an id into the
/// here.workers table.
///
/// NB: We could store either the id or a pointer to a worker_data_t as a thread
///     local, but that adds a level of indirection that only really makes sense
///     if we support more dynamism than we do (i.e., pthreads that come and go
///     and get attached to worker data).
///
/// NB: We could just as easily allocate individual thread locals for these
///     variables, but we use the structure in order to facilitate it's sharing
///     through the here.workers array, and to prepare for a world where we
///     might want to have a more dynamic ability to bind between workers and
///     their data.
static __thread worker_data_t me = {
  .id = -1,
  .padding = {0},
  .work = SYNC_CHASE_LEV_WS_DEQUE_INIT
};


/// The pthread entry point for all worker threads.
static void *worker_entry(void *args) {
  me.id = FINC(&here.next);                      // race to get an id
  assert(0 <= me.id && me.id < here.max);
  assert((uintptr_t)&me % HPX_CACHELINE_SIZE == 0);
  dbg_log("Worker thread %d initializing data.\n", me.id);
  sync_chase_lev_ws_deque_init(&me.work, 64);
  here.workers[me.id] = &me;
  dbg_log("Worker thread %d completed initialization.\n", me.id);
  sync_barrier_join(here.barrier, me.id);

  while (!LOAD(&here.shutdown))
    ;

  return NULL;
}


///
int cosched_init(int max_threads) {
  if (max_threads <= 0) {
    dbg_error("Must have at least one scheduler thread.");
    return LIBHPX_ERR;
  }

  here.max = max_threads;
  here.threads = calloc(here.max, sizeof(here.threads[0]));
  if (here.threads == NULL) {
    dbg_error("Could not allocate a scheduler thread array.");
    return LIBHPX_ERR;
  }
  here.workers = calloc(here.max, sizeof(here.workers[0]));
  if (here.workers == NULL) {
    dbg_error("Could not allocate a scheduler worker data array.");
    free(here.threads);
    return LIBHPX_ERR;
  }

  here.barrier = sr_barrier_new(here.max);
  if (here.barrier == NULL) {
    dbg_error("Could not allocate a scheduler barrier.");
    free(here.threads);
    free(here.workers);
    return LIBHPX_ERR;
  }

  return LIBHPX_OK;
}


/// Spawn the set of scheduler threads, and run the scheduler code.
///
/// NB: We currently spawn here.max threads. This might not be what we want to
///     do going forwards.
int cosched_run(void) {
  for (int i = 0, e = here.max - 1; i < e; ++i) {
    int e = pthread_create(&here.threads[i], NULL, worker_entry, NULL);
    dbg_check(e, "Failed to start cosched thread %d.", i);
  }

  here.threads[here.max - 1] = pthread_self();
  worker_entry(NULL);

  for (int i = 0, e = here.max - 1; i < e; ++i) {
    int e = pthread_join(here.threads[i], NULL);
    dbg_check(e, "Failed to join cosched thread %d.", i);
  }

  return LIBHPX_OK;
}
