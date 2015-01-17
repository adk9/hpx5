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
#include <string.h>
#include <mpi.h>

#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/transport.h"
#include "progress.h"


/// the MPI transport
typedef struct {
  transport_t class;
  progress_t *progress;
} mpi_t;


/// Get the ID for an MPI transport.
static const char *_mpi_id(void) {
  return "MPI";
}


/// Use MPI barrier directly.
static void _mpi_barrier(void) {
  MPI_Barrier(MPI_COMM_WORLD);
}


/// Return the size of an MPI request.
static int _mpi_request_size(void) {
  return sizeof(MPI_Request);
}


/// Return the size of the transport-specific registration key.
static int _mpi_rkey_size(void) {
  return 0;
}


static int _mpi_adjust_size(int size) {
  return size;
}


/// Cancel an active MPI request.
static int _mpi_request_cancel(void *request) {
  return MPI_Cancel(request);
}


/// Shut down MPI, and delete the transport.
static void _mpi_delete(transport_t *transport) {
  mpi_t *mpi = (mpi_t*)transport;
  network_progress_delete(mpi->progress);
  int finalized;
  MPI_Finalized(&finalized);
  if (!finalized)
    MPI_Finalize();
  free(transport);
}


/// Pinning not necessary.
static int _mpi_pin(transport_t *transport, const void* buffer,
                    size_t len) {
  return LIBHPX_OK;
}


/// Unpinning not necessary.
static void _mpi_unpin(transport_t *transport, const void* buffer,
                       size_t len) {
}


/// Put data via MPI_Put
static int _mpi_put(transport_t *t, int dest, const void *data, size_t n,
                    void *rbuffer, size_t rn, void *rid, void *r)
{
  dbg_log_trans("put unsupported.\n");
  return HPX_SUCCESS;
}


/// Get data via MPI_Get
static int _mpi_get(transport_t *t, int dest, void *buffer, size_t n,
                    const void *rdata, size_t rn, void *rid, void *r)
{
  dbg_log_trans("get unsupported.\n");
  return HPX_SUCCESS;
}


/// Send data via MPI.
///
/// Presumably this will be an "eager" send. Don't use "data" until it's done!
static int _mpi_send(transport_t *t, int dest, const void *data, size_t n,
                 void *r)
{
  void *b = (void*)data;
  int e = MPI_Isend(b, n, MPI_BYTE, dest, here->rank, MPI_COMM_WORLD, r);
  if (e != MPI_SUCCESS)
    return dbg_error("could not send %lu bytes to %i.\n", n, dest);
  return HPX_SUCCESS;
}


/// Probe MPI to see if anything has been received.
static size_t _mpi_probe(transport_t *transport, int *source) {
  if (*source != TRANSPORT_ANY_SOURCE) {
    dbg_error("cannot currently probe source %d.\n", *source);
    return 0;
  }

  int flag = 0;
  MPI_Status status;
  int e = MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag,
                     &status);

  if (e != MPI_SUCCESS) {
    dbg_error("failed iprobe %d.\n", e);
    return 0;
  }

  if (!flag)
    return 0;

  int bytes = 0;
  e = MPI_Get_count(&status, MPI_BYTE, &bytes);
  if (e != MPI_SUCCESS) {
    dbg_error("could not get count %d.\n", e);
    return 0;
  }

  // update the source to the actual source, and return the number of bytes
  // available
  *source = status.MPI_SOURCE;
  return bytes;
}


/// Receive a buffer.
static int _mpi_recv(transport_t *t, int src, void* buffer, size_t n, void *r)
{
  assert(src != TRANSPORT_ANY_SOURCE);
  assert(src >= 0);
  assert(src < here->ranks);

  int e = MPI_Irecv(buffer, n, MPI_BYTE, src, src, MPI_COMM_WORLD, r);
  if (e != MPI_SUCCESS)
    return dbg_error("could not receive %lu bytes from %i.\n", n, src);

  return HPX_SUCCESS;
}


static int _mpi_test(transport_t *t, void *request, int *success) {
  int e = MPI_Test(request, success, MPI_STATUS_IGNORE);
  if (e != MPI_SUCCESS)
    return dbg_error("failed MPI_Test %d.\n", e);
  return HPX_SUCCESS;
}


static void _mpi_progress(transport_t *t, transport_op_t op) {
  mpi_t *mpi = (mpi_t*)t;
  switch (op) {
  case TRANSPORT_FLUSH:
    network_progress_flush(mpi->progress);
    break;
  case TRANSPORT_POLL:
    network_progress_poll(mpi->progress);
    break;
  case TRANSPORT_CANCEL:
    break;
  default:
    break;
  }
}


static uint32_t _mpi_get_send_limit(transport_t *t) {
  return t->send_limit;
}


static uint32_t _mpi_get_recv_limit(transport_t *t) {
  return t->recv_limit;
}


transport_t *transport_new_mpi(uint32_t send_limit, uint32_t recv_limit) {
  int init = 0;
  MPI_Initialized(&init);
  if (!init) {
    static const int LIBHPX_THREAD_LEVEL = MPI_THREAD_FUNNELED;
    int level;
    int e = MPI_Init_thread(NULL, NULL, LIBHPX_THREAD_LEVEL, &level);
    if (e != MPI_SUCCESS) {
      dbg_error("mpi initialization failed\n");
      return NULL;
    }

    if (level != LIBHPX_THREAD_LEVEL) {
      dbg_error("MPI thread level failed requested %d, received %d.\n",
                LIBHPX_THREAD_LEVEL, level);
      return NULL;
    }

    dbg_log_trans("thread_support_provided = %d\n", level);
  }

  mpi_t *mpi = malloc(sizeof(*mpi));
  mpi->class.type           = HPX_TRANSPORT_MPI;
  mpi->class.id             = _mpi_id;
  mpi->class.barrier        = _mpi_barrier;
  mpi->class.request_size   = _mpi_request_size;
  mpi->class.rkey_size      = _mpi_rkey_size;
  mpi->class.request_cancel = _mpi_request_cancel;
  mpi->class.adjust_size    = _mpi_adjust_size;
  mpi->class.get_send_limit = _mpi_get_send_limit;
  mpi->class.get_recv_limit = _mpi_get_recv_limit;

  mpi->class.delete         = _mpi_delete;
  mpi->class.pin            = _mpi_pin;
  mpi->class.unpin          = _mpi_unpin;
  mpi->class.put            = _mpi_put;
  mpi->class.get            = _mpi_get;
  mpi->class.send           = _mpi_send;
  mpi->class.probe          = _mpi_probe;
  mpi->class.recv           = _mpi_recv;
  mpi->class.test           = _mpi_test;
  mpi->class.testsome       = NULL;
  mpi->class.progress       = _mpi_progress;

  mpi->class.send_limit     = (send_limit == 0) ? UINT16_MAX : send_limit;
  mpi->class.recv_limit     = (recv_limit == 0) ? UINT16_MAX : recv_limit;
  mpi->class.rkey_table     = NULL;

  mpi->progress             = network_progress_new(&mpi->class);
  if (!mpi->progress) {
    dbg_error("failed to start the progress loop.\n");
  }
  return &mpi->class;
}
