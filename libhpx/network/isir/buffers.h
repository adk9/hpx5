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
#ifndef LIBHPX_NETWORK_ISIR_BUFFERS_H
#define LIBHPX_NETWORK_ISIR_BUFFERS_H

#include <mpi.h>
#include <hpx/hpx.h>
#include <libsync/queues.h>

typedef struct {
  hpx_parcel_t *parcel;
  hpx_addr_t    local;
} record_t;

typedef hpx_parcel_t *(*finalizer_t)(hpx_parcel_t *p, MPI_Status *status);

typedef struct {
  uint32_t            n;
  uint32_t        limit;
  finalizer_t      fini;
  uint64_t          min;
  uint64_t       active;
  uint64_t          max;
  MPI_Request *requests;
  record_t     *records;
} buffer_t;


/// Initialize a buffer.
///
/// @param       buffer The buffer to initialize.
/// @param         size The initial size for the buffer.
/// @param        limit The limit of the number of active requests.
/// @param            f A callback to finalize a parcel.
///
/// @returns            LIBHPX_OK or an error code.
int buffer_init(buffer_t *buffer, uint32_t size, uint32_t limit, finalizer_t f)
  HPX_INTERNAL HPX_NON_NULL(1, 4);


/// Finalize a buffer.
///
/// All outstanding requests will be canceled.
///
/// @param       buffer The buffer to finalize.
void buffer_fini(buffer_t *buffer)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Double the size of an buffer.
///
/// @param       buffer The buffer to double.
void buffer_double(buffer_t *buffer)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Compute the index of an element.
///
/// Buffers are circular buffers implemented as arrays that need to be indexed
/// modulo the size of the array. We only use n^2 element arrays so that we can
/// do the modulo using a mask operation rather than the more expensive %
/// operation.
///
/// @param       buffer The buffer to index into.
/// @param            i The logical index of the element.
///
/// @returns            The physical index into the buffer array for @i.
static inline uint32_t buffer_index_of(buffer_t *buffer, uint64_t i) {
  uint64_t index = i & (buffer->n - 1);
  return (uint32_t)index;
}


/// Compact a buffer by eliminating the passed entry.
void buffer_compact(buffer_t *buffer, uint32_t i)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Tests all of the active requests in a buffer.
///
int buffer_test_all(buffer_t *buffer, hpx_parcel_t **completed)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Add an element to the buffer.
///
/// @param       buffer The buffer to append to.
/// @param       record The record to append.
void buffer_append(buffer_t *buffer, record_t record)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Initialize an isend buffer.
///
/// @param       buffer The buffer to initialize.
/// @param         size The initial size for the buffer.
/// @param        limit The limit of the number of active requests.
///
/// @returns            LIBHPX_OK or an error code.
int isend_buffer_init(buffer_t *buffer, uint32_t size, uint32_t limit)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Progress the isends in the buffer.
///
/// @param       buffer The buffer to progress().
///
/// @returns            The number of completed requests.
int isend_buffer_progress(buffer_t *buffer, two_lock_queue_t *parcels)
  HPX_INTERNAL HPX_NON_NULL(1);


/// Flush all outstanding sends.
///
/// This is synchronous and will not return until all of the buffered sends and
/// sends in the parcel queue have completed. It is not thread safe.
///
/// @param       buffer The send buffer to flush().
/// @param      parcels A queue of not-yet-visible parcels to flush.
void isend_buffer_flush(buffer_t *buffer, two_lock_queue_t *parcels)
  HPX_INTERNAL HPX_NON_NULL(1);


#endif // LIBHPX_NETWORK_ISIR_BUFFERS_H
