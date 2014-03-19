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
#include <config.h>
#endif

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <mpi.h>

#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "libhpx/transport.h"
#include "transports.h"

/// make sure all messages go through send/recv and not put/get (which are not
/// implemented)
static const int EAGER_THRESHOLD_MPI_DEFAULT = INT_MAX;

/// the MPI transport caches the number of ranks
typedef struct {
  transport_t vtable;
  int rank;
  int n_ranks;
} mpi_t;


/// ----------------------------------------------------------------------------
/// Get the ID for an MPI transport.
/// ----------------------------------------------------------------------------
static const char *_id(void) {
  return "MPI";
}


/// ----------------------------------------------------------------------------
/// Use MPI barrier directly.
/// ----------------------------------------------------------------------------
static void _barrier(void) {
  MPI_Barrier(MPI_COMM_WORLD);
}


/// ----------------------------------------------------------------------------
/// Return the size of an MPI request.
/// ----------------------------------------------------------------------------
static int _request_size(void) {
  return sizeof(MPI_Request);
}


static int _adjust_size(int size) {
  return size;
}


/// ----------------------------------------------------------------------------
/// Shut down MPI, and delete the transport.
/// ----------------------------------------------------------------------------
static void _delete(transport_t *transport) {
  int finalized;
  MPI_Finalized(&finalized);
  if (!finalized)
    MPI_Finalize();
  free(transport);
}


/// ----------------------------------------------------------------------------
/// Pinning not necessary.
/// ----------------------------------------------------------------------------
static void _pin(transport_t *transport, const void* buffer, size_t len) {
}


/// ----------------------------------------------------------------------------
/// Unpinning not necessary.
/// ----------------------------------------------------------------------------
static void _unpin(transport_t *transport, const void* buffer, size_t len) {
}


/// ----------------------------------------------------------------------------
/// Send data via MPI.
///
/// Presumably this will be an "eager" send. Don't use "data" until it's done!
/// ----------------------------------------------------------------------------
static int _send(transport_t *t, int dest, const void *data, size_t n, void *r)
{
  mpi_t *mpi = (mpi_t*)t;
  void *b = (void*)data;
  if (MPI_Isend(b, n, MPI_BYTE, dest, mpi->rank, MPI_COMM_WORLD, r) ==
      MPI_SUCCESS)
      return HPX_SUCCESS;

  dbg_error("MPI could not send %lu bytes to %i.\n", n, dest);
  return HPX_ERROR;
}


/// ----------------------------------------------------------------------------
/// Probe MPI to see if anything has been received.
/// ----------------------------------------------------------------------------
static size_t _probe(transport_t *transport, int *source) {
  if (*source != TRANSPORT_ANY_SOURCE) {
    dbg_error("mpi transport can not currently probe source %d\n", *source);
    return 0;
  }

  int flag = 0;
  MPI_Status status;
  if (MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status) !=
      MPI_SUCCESS) {
    dbg_error("mpi failed Iprobe.\n");
    return 0;
  }

  if (!flag)
    return 0;

  int bytes = 0;
  if (MPI_Get_count(&status, MPI_BYTE, &bytes) != MPI_SUCCESS) {
    dbg_error("could not extract bytes from mpi.\n");
    return 0;
  }

  // update the source to the actual source, and return the number of bytes
  // available
  *source = status.MPI_SOURCE;
  return bytes;
}


/// ----------------------------------------------------------------------------
/// Receive a buffer.
/// ----------------------------------------------------------------------------
static int _recv(transport_t *t, int src, void* buffer, size_t n, void *r) {
  mpi_t *mpi = (mpi_t*)t;

  assert(src != TRANSPORT_ANY_SOURCE);
  assert(src >= 0);
  assert(src < mpi->n_ranks);

  if (MPI_Irecv(buffer, n, MPI_BYTE, src, src, MPI_COMM_WORLD, r) ==
      MPI_SUCCESS)
    return HPX_SUCCESS;

  dbg_error("could not receive %lu bytes from %i", n, src);
  return HPX_ERROR;
}


transport_t *transport_new_mpi(const boot_t *boot) {
  int val = 0;
  MPI_Initialized(&val);

  if (!val) {
    int threading = 0;
    if (MPI_Init_thread(NULL, NULL, MPI_THREAD_SERIALIZED, &threading) !=
        MPI_SUCCESS)
      return NULL;

    dbg_log("thread_support_provided = %d\n", threading);
  }

  mpi_t *mpi = malloc(sizeof(*mpi));
  mpi->vtable.id           = _id;
  mpi->vtable.barrier      = _barrier;
  mpi->vtable.request_size = _request_size;
  mpi->vtable.adjust_size  = _adjust_size;

  mpi->vtable.delete       = _delete;
  mpi->vtable.pin          = _pin;
  mpi->vtable.unpin        = _unpin;
  mpi->vtable.send         = _send;
  mpi->vtable.probe        = _probe;
  mpi->vtable.recv         = _recv;

  mpi->rank                = boot_rank(boot);
  mpi->n_ranks             = boot_n_ranks(boot);

  return &mpi->vtable;
}

// LD: below archived temporarily
#if 0
/* status may be NULL */
static int _test(transport_request_t *request, int *flag, transport_status_t *status) {
  MPI_Status *s = (status) ? &(status->mpi) : MPI_STATUS_IGNORE;
  int test_result = MPI_Test(&(request->mpi), flag, s);

  if (test_result != MPI_SUCCESS)
    return (__hpx_errno = HPX_ERROR);

  if (!status)
    return HPX_SUCCESS;

  if (*flag == true)
    status->source = status->mpi.MPI_SOURCE;

  return HPX_SUCCESS;
}

static int _put(int dest, void *buffer, size_t len,
                transport_request_t *request) {
  return HPX_ERROR;
}

static int _get(int src, void *buffer, size_t len, transport_request_t *request)
{
  return HPX_ERROR;
}

/* Return the physical transport ID of the current process */
static int _phys_addr(int *l) {
  int ret;
  ret = HPX_ERROR;

  if (!l) {
    /* TODO: replace with more specific error */
    __hpx_errno = HPX_ERROR;
    return ret;
  }

  l->rank = rank;
  return 0;
}


static void _progress(void *data) {
}


static int _fini(void) {
  int retval;
  int temp;
  retval = HPX_ERROR;

  MPI_Finalized(&retval);
  if (!retval) {
    temp = MPI_Finalize();

    if (temp == MPI_SUCCESS)
      retval = 0;
    else
      __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
  }

  free(_argv_buffer);
  free(_argv);

  return retval;
}
#endif
