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
#include "progress.h"
#include "transports.h"


/// the MPI transport caches the number of ranks
typedef struct {
  transport_t  vtable;
  int          rank;
  int          n_ranks;
  progress_t  *progress;
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
/// Cancel an active MPI request.
/// ----------------------------------------------------------------------------
static int _request_cancel(void *request) {
  return MPI_Cancel(request);
}


/// ----------------------------------------------------------------------------
/// Shut down MPI, and delete the transport.
/// ----------------------------------------------------------------------------
static void _delete(transport_t *transport) {
  mpi_t *mpi = (mpi_t*)transport;
  network_progress_delete(mpi->progress);
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
  int e = MPI_Isend(b, n, MPI_BYTE, dest, mpi->rank, MPI_COMM_WORLD, r);
  if (e != MPI_SUCCESS)
    return dbg_error("MPI could not send %lu bytes to %i.\n", n, dest);
  return HPX_SUCCESS;
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
  int e = MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag,
                     &status);

  if (e != MPI_SUCCESS) {
    dbg_error("mpi failed Iprobe.\n");
    return 0;
  }

  if (!flag)
    return 0;

  int bytes = 0;
  e = MPI_Get_count(&status, MPI_BYTE, &bytes);
  if (e != MPI_SUCCESS) {
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

  int e = MPI_Irecv(buffer, n, MPI_BYTE, src, src, MPI_COMM_WORLD, r);
  if (e != MPI_SUCCESS)
    return dbg_error("could not receive %lu bytes from %i", n, src);

  return HPX_SUCCESS;
}


static int _test(transport_t *t, void *request, int *success) {
  int e = MPI_Test(request, success, MPI_STATUS_IGNORE);
  if (e != MPI_SUCCESS)
    return dbg_error("failed MPI_Test.\n");

  return HPX_SUCCESS;
}

static void _progress(transport_t *t, bool flush) {
  mpi_t *mpi = (mpi_t*)t;
  network_progress_poll(mpi->progress);
  if (flush)
    network_progress_flush(mpi->progress);
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
  mpi->vtable.id             = _id;
  mpi->vtable.barrier        = _barrier;
  mpi->vtable.request_size   = _request_size;
  mpi->vtable.request_cancel = _request_cancel;
  mpi->vtable.adjust_size    = _adjust_size;

  mpi->vtable.delete         = _delete;
  mpi->vtable.pin            = _pin;
  mpi->vtable.unpin          = _unpin;
  mpi->vtable.send           = _send;
  mpi->vtable.probe          = _probe;
  mpi->vtable.recv           = _recv;
  mpi->vtable.test           = _test;
  mpi->vtable.progress       = _progress;

  mpi->rank                  = boot_rank(boot);
  mpi->n_ranks               = boot_n_ranks(boot);

  mpi->progress              = network_progress_new(boot, &mpi->vtable);
  if (!mpi->progress) {
    dbg_error("failed to start the transport progress loop.\n");
    hpx_abort();
  }
  return &mpi->vtable;
}
