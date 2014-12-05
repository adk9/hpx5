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


#include <hpx/builtins.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include "buffers.h"


/// Add all of the available elements in the queue to the buffer.
///
/// This is just a temporary design, as it ties us too closely to the funneled
/// network.
///
/// @param       isends The buffer to append to.
/// @param        sends The queue of parcels to append.
static void _append_all(buffer_t *isends, two_lock_queue_t *sends) {
  hpx_parcel_t *p = NULL;
  while ((p = sync_two_lock_queue_dequeue(sends))) {
    record_t record = {
      .parcel = p,
      .local = HPX_NULL
    };
    buffer_append(isends, record);
  }
}


/// Start an isend operation.
///
/// The isend buffer keeps two ranges, [min, active) and [active, max). This
/// operation issues an MPI_Isend for the element at the active index, and
/// increments the active index.
///
/// @precondition There must be a valid entry in the buffer that is not yet
///               active.
///
/// @param       isends The buffer to start the send from.
/// @param          gas The global address space used for address translation.
///
/// @returns  LIBHPX_OK success
///        LIBHPX_RETRY MPI internal error
///        LIBHPX_ERROR other error
static int _start_isend(buffer_t *isends, gas_class_t *gas) {
  assert(isends->active < isends->max);

  static const    int OFFSET = sizeof(void*) + 2 * sizeof(int);
  static const       int TAG = 0;
  static const MPI_Comm COMM = MPI_COMM_WORLD;

  uint64_t            i = isends->active++;
  uint32_t            j = buffer_index_of(isends, i);
  MPI_Request      *req = &isends->requests[j];
  const hpx_parcel_t *p = isends->records[j].parcel;
  void            *data = (void*)&p->action;
  unsigned            n = sizeof(*p) + p->size - OFFSET;
  int              rank = gas_owner_of(gas, p->target);

  DEBUG_IF(true) {
    *req = MPI_REQUEST_NULL;
  }

  int e = MPI_Isend(data, n, MPI_BYTE, rank, TAG, COMM, req);
  if (e == MPI_SUCCESS) {
    dbg_log_net("started MPI_Isend: %u bytes to %d\n", n, rank);
    return LIBHPX_OK;
  }

  if (e == MPI_ERR_INTERN) {
    dbg_log_net("MPI_Isend encountered an internal error: %d\n", e);
    return LIBHPX_RETRY;
  }

  return dbg_error("failed MPI_Isend: %u bytes to %d (%d)\n", n, rank, e);
}


/// Start as many isend operations as we can.
///
/// @param       isends The buffer containing the requests that we want to
///                     start.
///
/// @returns  LIBHPX_OK no errors were encountered
///        LIBHPX_ERROR we encountered an error that we couldn't handle
static int _start_all(buffer_t *isends) {
  int n = isends->max - isends->active;
  if (isends->limit) {
    n =  min_int(n, isends->limit);
  }
  for (int i = 0; i < n; ++i) {
    int e = _start_isend(isends, here->gas);
    switch (e) {
     case LIBHPX_OK:
      break;
     case LIBHPX_RETRY:
      return LIBHPX_OK;
     default:
      return LIBHPX_ERROR;
    }
  }
  return LIBHPX_OK;
}


/// Finish an isend operation.
///
/// This is used as a finalizer for the buffer. Currently, finishing an isend
/// operation just requires that we free the buffer associated with the parcel.
///
/// @param            p The parcel (i.e., buffer) associated with the isend.
/// @param            s The status of the isend operation.
///
/// @returns          NULL
static hpx_parcel_t *_finish_isend(hpx_parcel_t *p, MPI_Status *s) {
  hpx_parcel_release(p);
  return NULL;
}


int isend_buffer_init(buffer_t *buffer, uint32_t size, uint32_t limit) {
  return buffer_init(buffer, size, limit, _finish_isend);
}


void isend_buffer_flush(buffer_t *buffer, two_lock_queue_t *parcels) {
  // always progress at least once to empty the parcels queue
  do {
    isend_buffer_progress(buffer, parcels);
  } while (buffer->min != buffer->max);
  assert(sync_two_lock_queue_dequeue(parcels) == NULL);
}


int isend_buffer_progress(buffer_t *isends, two_lock_queue_t *parcels) {
  // Test all of the active isends in the buffer. We know that the isend
  // finalizer doesn't create an output list, but we still need a non-null
  // hpx_parcel_t* for the call.
  hpx_parcel_t *unused = NULL;
  int n = buffer_test_all(isends, &unused);
  assert(!unused);

  // Move all of the isends that are pending in the parcel queue into the
  // buffer.
  _append_all(isends, parcels);

  // Start as many isend operations as possible from the buffer.
  _start_all(isends);

  // Return the number of isend operations that we completed.
  return n;
}
