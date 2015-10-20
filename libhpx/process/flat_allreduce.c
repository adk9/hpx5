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

#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include <libhpx/worker.h>
#include "flat_reduce.h"
#include "flat_continuation.h"
#include "map_reduce.h"

// At each locality I need a flat_continuation, and to know the flat_reduce
// address to reduce into. For now I'm allocating local memory for these
// structures, since we're using the 2-sided parcel transport to interact with
// them, and they aren't going to move around.
typedef struct {
  hpx_addr_t   reduce;
  void *continuations;
} _allreduce_t;

/// Initialize an element in an allreduce array.
///
/// @param            r The local element in the array.
/// @param       reduce The global address of the reduce tree.
static int _allreduce_init_handler(_allreduce_t *r, hpx_addr_t reduce) {
  r->reduce = reduce;
  r->continuations = flat_continuation_new();
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, HPX_PINNED, _allreduce_init,
                     _allreduce_init_handler, HPX_POINTER, HPX_ADDR);

/// Finalize an element in an allreduce array.
static int _allreduce_fini_handler(_allreduce_t *r) {
  flat_continuation_delete(r->continuations);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, HPX_PINNED, _allreduce_fini,
                     _allreduce_fini_handler, HPX_POINTER);

/// The allreduce delete operation.
///
/// We want to perform the allreduce asynchronously at the outermost layer, but
/// it requires some blocking and complex dataflow. We could build this with
/// some call-when-with-continuation work, but it's easier just to do it in a
/// thread.
///
/// If the client really wants to wait they can use a continuation to do
/// that.
static int _allreduce_delete_handler(_allreduce_t *r) {
  // Finalize and free the root reduction.
  dbg_check( hpx_call_sync(r->reduce, flat_reduce_fini, NULL, 0) );
  hpx_gas_free_sync(r->reduce);

  // Finalize all of the continuation elements.
  hpx_addr_t allreduce = hpx_thread_current_target();
  hpx_addr_t and = hpx_lco_and_new(here->ranks);
  dbg_check( map_reduce(_allreduce_fini, allreduce, here->ranks, sizeof(*r), 0,
                        hpx_lco_set_action, and) );
  dbg_check( hpx_lco_wait(and) );
  hpx_lco_delete(and, HPX_NULL);

  // Free the continuation array---use call_cc() because we currently have the
  // root of the continuation array pinned.
  hpx_call_cc(allreduce, hpx_gas_free_action, NULL, NULL);
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, _allreduce_delete,
                     _allreduce_delete_handler, HPX_POINTER);

/// The handler for broadcasting the reduced value to all of the waiting nodes.
///
/// This marshaled action handles the allreduce broadcast phase. It just
/// triggers the continuation structure stored at the local element, passing in
/// the marshaled value.
///
/// @param            r The allreduce element.
/// @param            v The value we are broadcasting.
/// @param            n The size of the value.
static int _allreduce_bcast_handler(_allreduce_t *r, const void *v, size_t n) {
  flat_continuation_trigger(r->continuations, v, n);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, HPX_PINNED | HPX_MARSHALLED,
                     _allreduce_bcast, _allreduce_bcast_handler,
                     HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

/// Join the allreduce.
static int _allreduce_join_handler(void *r, void *v, size_t n) {
  // if we're the last to arrive, broadcast the reduced value to the distributed
  // continuation array
  if (flat_reduce_join(r, v, v)) {
    hpx_addr_t bcast = self->current->c_target;
    size_t bsize = sizeof(_allreduce_t);
    int ranks = here->ranks;
    dbg_check( map(_allreduce_bcast, bcast, ranks, bsize, 0, v, n) );
  }

  // squash the continuation---this abuses the parcel infrastructure, but we
  // know what we're doing
  self->current->c_action = HPX_ACTION_NULL;
  self->current->c_target = HPX_NULL;
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, HPX_PINNED | HPX_MARSHALLED,
                     _allreduce_join, _allreduce_join_handler,
                     HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

/// Allocate a process allreduce.
///
/// The reduce phase uses a centralized flat reduce, and the broadcast phase is
/// implemented using a distributed cyclic array of flat continuations.
hpx_addr_t hpx_process_collective_allreduce_new(int inputs, size_t bytes,
                                                hpx_action_t id,
                                                hpx_action_t op) {

  // Start by allocating and initializing the flat allreduce. This will be
  // allocated local to whichever rank is calling the new() operation, unless we
  // are out of memory.
  size_t bsize = flat_reduce_size(bytes);
  hpx_addr_t reduce = hpx_gas_alloc_local(bsize, HPX_CACHELINE_SIZE);
  dbg_assert(reduce);
  dbg_check( hpx_call_sync(reduce, flat_reduce_init, NULL, 0, &inputs, &bytes,
                           &id, &op) );

  // Allocate the distributed array of allreduce elements, and initialize them
  // all with a map-reduce.
  bsize = sizeof(_allreduce_t);
  hpx_addr_t allreduce = hpx_gas_alloc_cyclic(here->ranks, bsize, 0);
  dbg_assert(allreduce);
  hpx_addr_t and = hpx_lco_and_new(here->ranks);
  dbg_assert(and);
  dbg_check( map_reduce(_allreduce_init, allreduce, here->ranks, bsize, 0,
                        hpx_lco_set_action, and, &reduce) );
  return allreduce;
}

/// Free an allreduce LCO.
///
/// This performs an asychronous delete operation, but we could expose the
/// ability to wait if we needed it.
void hpx_process_collective_allreduce_delete(hpx_addr_t allreduce) {
  dbg_check( hpx_call(allreduce, _allreduce_delete, HPX_NULL) );
}

/// Join a flat allreduce.
int hpx_process_collective_allreduce_join(hpx_addr_t allreduce,
                                          int id, size_t bytes, const void *in,
                                          hpx_action_t c_action,
                                          hpx_addr_t c_target) {
  // pin the local element of the allreduce
  size_t bsize = sizeof(_allreduce_t);
  hpx_addr_t element = hpx_addr_add(allreduce, here->rank * bsize, bsize);
  _allreduce_t *r = NULL;
  if (!hpx_gas_try_pin(element, (void*)&r)) {
    dbg_error("failed to pin the local element of an allreduce\n");
  }

  // create a parcel for the broadcast reply bit, and enqueue it in the
  // distributed continuation
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, bytes);
  p->action = c_action;
  p->target = c_target;
  flat_continuation_wait(r->continuations, p);

  // and join the reduce (abusing the continuation here)
  hpx_addr_t reduce = r->reduce;
  hpx_gas_unpin(element);
  return hpx_call(reduce, _allreduce_join, allreduce, in, bytes);
}

int hpx_process_collective_allreduce_join_sync(hpx_addr_t target, int id,
                                               size_t bytes, const void *in,
                                               void *out) {
  hpx_addr_t future = hpx_lco_future_new(bytes);
  dbg_assert(future);
  hpx_process_collective_allreduce_join(target, id, bytes, in,
                                        hpx_lco_set_action, future);
  int e = hpx_lco_get(future, bytes, out);
  hpx_lco_delete(future, HPX_NULL);
  return e;
}
