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
#include <libhpx/libhpx.h>
#include <libhpx/parcel.h>
#include "buffers.h"

/// Some constants that we use in this file.
/// @{
static const       int _TAG = MPI_ANY_TAG;
static const    int _SOURCE = MPI_ANY_SOURCE;
static const MPI_Comm _COMM = MPI_COMM_WORLD;
static const    int _OFFSET = sizeof(void*) + 2 * sizeof(int);
/// @}


/// Compute the number of bytes we need to send, given a parcel payload size.
///
/// We have to send some of the parcel structure as a header along with the
/// payload. The parcel structure in libhpx/parcel.h is carefully designed so
/// that we're not sending any extra stuff.
///
/// @param parcel_bytes The number of payload bytes in the parcel.
///
/// @returns            The number of bytes we have to send across the network
///                     for this parcel (parcel_bytes + header bytes).
static unsigned _mpi_bytes(uint32_t parcel_bytes) {
  return (sizeof(hpx_parcel_t) - _OFFSET) + parcel_bytes;
}


/// Compute the parcel payload size from an MPI recv size.
///
/// We need to initialize the size field for received parcels, given the number
/// of bytes we received from MPI. Essentially, we need to subtract the number
/// of header bytes from the MPI size.
///
/// @param    mpi_bytes The number of bytes we received from MPI.
///
/// @returns            The number of parcel payload bytes we received.
static uint32_t _parcel_bytes(int mpi_bytes) {
  int bytes = mpi_bytes - (sizeof(hpx_parcel_t) - _OFFSET);
  assert(bytes >= 0);
  return bytes;
}


/// Append an irecv request and record for a given parcel payload size.
///
/// The request will not become active at this point, but can become active due
/// to a _start_irecv() in the future.
///
/// @param       irecvs The buffer to append to.
/// @param parcel_bytes The size of the parcel we need
static int _append_record(buffer_t *irecvs, uint32_t parcel_bytes) {
  record_t r = {
    .parcel = hpx_parcel_acquire(NULL, parcel_bytes),
    .local = HPX_NULL
  };
  buffer_append(irecvs, r);
  return LIBHPX_OK;
}


/// This is passed to parcel_stack_foreach() once we have tested all of irecvs
/// in a buffer.
///
/// We replace each successful irecv operation with an identical irecv, with the
/// expectation that a successful irecv is a good indicator that we will see an
/// identical payload in the future.
///
/// @param            p The parcel we're going to clone.
/// @param          env The irecv buffer.
static void _regenerate_irecv(hpx_parcel_t *p, void *env) {
  dbg_log("regenerating irecv for %u-byte payloads\n", p->size);
  _append_record(env, p->size);
}


/// Probe for unexpected messages.
///
/// We do not have prior knowledge of the message sizes that we will receive. In
/// order to detect messages that have not yet matched an active irecv, we use
/// the MPI_Iprobe operation.
///
/// @param       irecvs The irecv buffer.
///
/// @returns  LIBHPX_OK
///        LIBHPX_ERROR We encountered an MPI error during the Iprobe
static int _probe(buffer_t *irecvs) {
  int flag = 0;
  MPI_Status status;
  int e = MPI_Iprobe(_SOURCE, _TAG, _COMM, &flag, &status);
  if (e != MPI_SUCCESS) {
    return dbg_error("failed iprobe %d.\n", e);
  }

  if (!flag) {
    return LIBHPX_OK;
  }

  // There's a pending message that we haven't matched yet. Append a record to
  // match it in the future.
  int mpi_bytes = 0;
  e = MPI_Get_count(&status, MPI_BYTE, &mpi_bytes);
  if (e != MPI_SUCCESS) {
    return dbg_error("could not get count %d.\n", e);
  }

  uint32_t payload_bytes = _parcel_bytes(mpi_bytes);
  dbg_log_net("added irecv for %u-byte payloads\n", payload_bytes);
  return _append_record(irecvs, payload_bytes);
}


/// Finish an irecv operation.
///
/// This is used as a finalizer in the buffer.
///
/// In order to finish an Irecv, we need to extract the source and size from the
/// status object and set those fields in the parcel header. We then return the
/// patched parcel so that it can be chained and returned to the application for
/// processing.
///
/// @param            p The parcel we received.
/// @param            s The MPI status object that we can extract the parcel's
///                     source and size from.
///
/// @returns            The parcel with its source and size filled out.
static hpx_parcel_t *_finish_irecv(hpx_parcel_t *p, MPI_Status *s) {
  if (s->MPI_ERROR != MPI_SUCCESS) {
    dbg_error("failed irecv\n");
  }

  int n = 0;
  int e = MPI_Get_count(s, MPI_BYTE, &n);
  if (e != MPI_SUCCESS || n < 0) {
    dbg_error("could not extract the size of an irecv\n");
  }

  p->src = s->MPI_SOURCE;
  p->size = _parcel_bytes(n);
  dbg_log_net("finished a recv for a %u-byte payload\n", p->size);
  return p;
}


/// Start an irecv operation.
///
/// @precondition There must be at least one request record in the buffer that
///               is not yet active.
///
/// @param       irecvs The buffer to start the irecv from.
///
/// @returns  LIBHPX_OK
///        LIBHPX_ERROR The MPI_Irecv returned an error code.
static int _start_irecv(buffer_t *irecvs) {
  assert(irecvs->active < irecvs->max);

  uint64_t       i = irecvs->active++;
  uint32_t       j = buffer_index_of(irecvs, i);
  MPI_Request *req = &irecvs->requests[j];
  hpx_parcel_t  *p = irecvs->records[j].parcel;
  void       *data = (void*)&p->action;
  unsigned       n = _mpi_bytes(p->size);

  DEBUG_IF(true) {
    *req = MPI_REQUEST_NULL;
  }

  int e = MPI_Irecv(data, n, MPI_BYTE, _SOURCE, _TAG, _COMM, req);
  if (e != MPI_SUCCESS) {
    return dbg_error("could not start irecv\n");
  }

  dbg_log("started an MPI_Irecv operation (%p): %u bytes\n", *req, n);
  return LIBHPX_OK;
}


/// Start as many irecvs as possible.
///
/// @param       irecvs The buffer to start requests from.
///
/// @returns  LIBHPX_OK
///        LIBHPX_ERROR We encountered an error when starting a request.
static int _start_all(buffer_t *irecvs) {
  int n = irecvs->max - irecvs->active;
  if (irecvs->limit) {
    n =  min_int(n, irecvs->limit);
  }
  for (int i = 0; i < n; ++i) {
    int e = _start_irecv(irecvs);
    if (e != LIBHPX_OK) {
      return e;
    }
  }
  return LIBHPX_OK;
}


int irecv_buffer_init(buffer_t *buffer, uint32_t size, uint32_t limit) {
  return buffer_init(buffer, size, limit, _finish_irecv);
}


int irecv_buffer_progress(buffer_t *irecvs, two_lock_queue_t *parcels) {
  hpx_parcel_t *completed = NULL;
  int n = buffer_test_all(irecvs, &completed);

  // it makes a lot of sense to continue to recv sizes that we've received
  // before, so we append clones for every successful receive
  parcel_stack_foreach(completed, irecvs, _regenerate_irecv);

  // output the completed parcels
  sync_two_lock_queue_enqueue(parcels, completed);

  // probe for any unexpected sizes
  _probe(irecvs);

  // and activate as many records as possible
  _start_all(irecvs);

  return n;
}
