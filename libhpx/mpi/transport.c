/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  MPI Network Interface
  mpi.c

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#ifdef __linux__
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <mpi.h>

#include "debug.h"                              /* dbg_ stuff */
#include "transport.h"

#define EAGER_THRESHOLD_MPI_DEFAULT INT_MAX /* make sure all messages go through
                                               send/recv and not put/get (which
                                               are not implemented) */

/* TODO: make reasonable once we have puts/gets working */

static int _init(void);
static int _finalize(void);
static void _progress(void *data);
static int _probe(int source, int* flag, transport_status_t* status);
static int _send(int dest, void *data, size_t len, transport_request_t *request);
static int _recv(int src, void *buffer, size_t len, transport_request_t *request);
static int _test(transport_request_t *request, int *flag, transport_status_t *status);
static int _put(int dest, void *buffer, size_t len, transport_request_t *request);
static int _get(int src, void *buffer, size_t len, transport_request_t *request);
static int _pin(void* buffer, size_t len);
static int _unpin(void* buffer, size_t len);
static int _hys_addr(hpx_locality_t *id);
static int _get_transport_bytes(size_t n);
static void _barrier(void);

transport_t *
mpi_new(void) {
/* MPI transport operations */
  transport_t *mpi = malloc(sizeof(*mpi));
  mpi->init                = _init;
  mpi->finalize            = _finalize;
  mpi->progress            = _progress;
  mpi->probe               = _probe;
  mpi->send                = _send;
  mpi->recv                = _recv;
  mpi->sendrecv_test       = _test;
  mpi->send_test           = _test;
  mpi->recv_test           = _test;
  mpi->put                 = _put;
  mpi->get                 = _get;
  mpi->putget_test         = _test;
  mpi->put_test            = _test;
  mpi->get_test            = _test;
  mpi->pin                 = _pin;
  mpi->unpin               = _unpin;
  mpi->phys_addr           = _phys_addr;
  mpi->get_transport_bytes = _get_transport_bytes;
  mpi->barrier             = _barrier;
  return mpi;
}

void
mpi_delete(transport_t *mpi) {
  free(mpi);
}

static int _eager_threshold_mpi = EAGER_THRESHOLD_MPI_DEFAULT;
static int _rank = -1;
static int _size = -1;

static char **_argv = NULL;
static char *_argv_buffer = NULL;

int _init(void) {
  int val = 0;
  MPI_Initialized(&val);

  if (!retval) {
    int temp = MPI_Init_thread(NULL, NULL, MPI_THREAD_SERIALIZED, &thread_support_provided);
    if (temp == MPI_SUCCESS)
      retval = 0;
    else
      __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */
    trace("thread_support_provided = %d\n", thread_support_provided);
  }
  else
    retval = HPX_SUCCESS;

  /* cache size and rank */
  rank = bootmgr->get_rank();
  size = bootmgr->size();

  return retval;
}

/* status may NOT be NULL */
int
probe(int source, int* flag, transport_status_t* status)
{
  int retval;
  int temp;
  int mpi_src = -1;
  /* int mpi_len; LD:unused */

  retval = HPX_ERROR;
  if (source == TRANSPORT_ANY_SOURCE)
    mpi_src = MPI_ANY_SOURCE;

  temp = MPI_Iprobe(mpi_src, MPI_ANY_TAG, MPI_COMM_WORLD, flag, &(status->mpi));

  if (temp == MPI_SUCCESS) {
    retval = 0;
    if (*flag == true) {
      status->source = status->mpi.MPI_SOURCE;
      MPI_Get_count(&(status->mpi), MPI_BYTE, &(status->count));
    }
  }
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;
}

/* Send data via MPI. Presumably this will be an "eager" send. Don't use "data" until it's done! */
int
send(int dest, void *data, size_t len, transport_request_t *request)
{
  int retval;
  int temp;

  retval = HPX_ERROR;

  /* TODO: move this when checking for eager_threshhold boundary */
  if (len > INT_MAX) {
    __hpx_errno = HPX_ERROR;
    retval = HPX_ERROR;
  }
  /* not necessary because of eager_threshold */
#if 0
  /* TODO: put this back in - but maybe make this automatically call put() in place of the send */
  if (len > eager_threshold_mpi) { /* need to use _transport_put_* for that */
    __hpx_errno = HPX_ERROR;
    retval = HPX_ERROR;
  }
#endif

  temp = MPI_Isend(data, (int)len, MPI_BYTE, dest, rank, MPI_COMM_WORLD, &(request->mpi));

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

  return retval;
}

/* this is non-blocking recv - user must test/wait on the request */
int
recv(int source, void* buffer, size_t len, transport_request_t *request)
{
  int retval;
  int temp;
  int tag;
  int mpi_src;
  int mpi_len;

  retval = HPX_ERROR;
  if (source == TRANSPORT_ANY_SOURCE) {
    mpi_src = MPI_ANY_SOURCE;
    tag = 0;
  }
  else {
    mpi_src = source;
    tag = source;
  }

  /* This may go away eventually. If we take this out, we need to use MPI_Get_count to get the size (which introduces problems with threading, should we ever change that) or we need to change sending to send the size first. */
  if (len == TRANSPORT_ANY_LENGTH)
    mpi_len = eager_threshold_mpi;
  else {
    if (len > eager_threshold_mpi) { /* need to use _transport_put_* for that */
      __hpx_errno = HPX_ERROR;
      retval = HPX_ERROR;
      goto error;
    }
    else
      mpi_len = len;
  }

  temp = MPI_Irecv(buffer, (int)mpi_len, MPI_BYTE, mpi_src, tag, MPI_COMM_WORLD, &(request->mpi));

  if (temp == MPI_SUCCESS)
    retval = 0;
  else
    __hpx_errno = HPX_ERROR; /* TODO: replace with more specific error */

error:
  return retval;
}

/* status may be NULL */
int
test(transport_request_t *request, int *flag, transport_status_t *status)
{
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

int
put(int dest, void *buffer, size_t len, transport_request_t *request)
{
  return HPX_ERROR;
}

int
get(int src, void *buffer, size_t len, transport_request_t *request)
{
  return HPX_ERROR;
}

/* Return the physical transport ID of the current process */
int
phys_addr(hpx_locality_t *l)
{
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

void
progress(void *data)
{
}

int
finalize(void)
{
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

int
pin(void* buffer, size_t len)
{
  dbg_assert_precondition(len && buffer);
  dbg_printf("%d: Pinning %zd bytes at %p (MPI no-op)\n",
             hpx_get_rank(), len, buffer);
  return 0;
}

int
unpin(void* buffer, size_t len)
{
  dbg_assert_precondition(len && buffer);
  dbg_printf("%d: Unpinning/freeing %zd bytes from buffer at %p (MPI no-op)\n",
             hpx_get_rank(), len, buffer);
  return 0;
}

size_t
get_transport_bytes(size_t n)
{
  return n;
}

void
barrier(void)
{
#if HAVE_MPI
  MPI_Barrier(MPI_COMM_WORLD);
#endif
}
