// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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
#include <string.h>
#include <mpi.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/memory.h>
#include "parcel_utils.h"
#include "xport.h"

static const int LIBHPX_THREAD_LEVEL = MPI_THREAD_SERIALIZED;

typedef struct {
  isir_xport_t vtable;
  MPI_Comm       comm;
  int            fini;
} _mpi_xport_t;

static void
_mpi_check_tag(const void *xport, int tag)
{
  const _mpi_xport_t *mpi = xport;
  int *tag_ub;
  int flag = 0;
  int e = MPI_Comm_get_attr(mpi->comm, MPI_TAG_UB, &tag_ub, &flag);
  dbg_check(e, "Could not extract tag upper bound\n");
  dbg_assert_str(*tag_ub > tag, "tag value out of bounds (%d > %d)\n", tag,
                 *tag_ub);
}

static size_t
_mpi_sizeof_request(void)
{
  return sizeof(MPI_Request);
}

static size_t
_mpi_sizeof_status(void)
{
  return sizeof(MPI_Status);
}

static int
_mpi_isend(const void *xport, int to, const void *from, unsigned n, int tag,
           void *r)
{
  const _mpi_xport_t *mpi = xport;
  int e = MPI_Isend((void *)from, n, MPI_BYTE, to, tag, mpi->comm, r);
  if (MPI_SUCCESS != e) {
    return log_error("failed MPI_Isend: %u bytes to %d\n", n, to);
  }

  log_net("started MPI_Isend: %u bytes to %d\n", n, to);
  return LIBHPX_OK;
}

static int
_mpi_irecv(const void *xport, void *to, size_t n, int tag, void *request) {
  const _mpi_xport_t *mpi = xport;
  const int src = MPI_ANY_SOURCE;
  const MPI_Comm com = mpi->comm;
  if (MPI_SUCCESS != MPI_Irecv(to, n, MPI_BYTE, src, tag, com, request)) {
    return log_error("could not start irecv\n");
  }
  return LIBHPX_OK;
}

static int
_mpi_iprobe(const void *xport, int *tag) {
  const _mpi_xport_t *mpi = xport;
  int flag;
  MPI_Status stat;
  const int src = MPI_ANY_SOURCE;
  const MPI_Comm comm = mpi->comm;
  if (MPI_SUCCESS != MPI_Iprobe(src, MPI_ANY_TAG, comm, &flag, &stat)) {
    return log_error("failed MPI_Iprobe\n");
  }

  *tag = -1;

  if (flag) {
    *tag = stat.MPI_TAG;
    log_net("probe detected irecv for %u-byte parcel\n",
            tag_to_payload_size(*tag));
  }
  return LIBHPX_OK;
}

static void
_mpi_testsome(int n, void *requests, int *nout, int *out, void *stats)
{
  if (!stats) {
    stats = MPI_STATUS_IGNORE;
  }

  int e = MPI_Testsome(n, requests, nout, out, stats);
  dbg_assert_str(e == MPI_SUCCESS, "MPI_Testsome error is fatal.\n");
  dbg_assert_str(*nout != MPI_UNDEFINED, "silent MPI_Testsome() error.\n");
  (void)e;
}

static void
_mpi_clear(void *request)
{
  MPI_Request *r = request;
  *r = MPI_REQUEST_NULL;
}

static int
_mpi_cancel(void *request, int *cancelled)
{
  MPI_Request *r = request;
  if (*r == MPI_REQUEST_NULL) {
    *cancelled = 1;
    return LIBHPX_OK;
  }

  if (MPI_SUCCESS != MPI_Cancel(request)) {
    return log_error("could not cancel MPI request\n");
  }

  MPI_Status status;
  if (MPI_SUCCESS != MPI_Wait(request, &status)) {
    return log_error("could not cleanup a canceled MPI request\n");
  }

  int c;
  if (MPI_SUCCESS != MPI_Test_cancelled(&status, (cancelled) ? cancelled : &c)) {
    return log_error("could not test a status to see if a request was canceled\n");
  }

  return LIBHPX_OK;
}

static void
_mpi_finish(void *status, int *src, int *bytes)
{
  MPI_Status *s = status;
  if (MPI_SUCCESS != MPI_Get_count(s, MPI_BYTE, bytes)) {
    dbg_error("could not extract the size of an irecv\n");
  }

  dbg_assert(*bytes > 0);
  *src = s->MPI_SOURCE;
}

static void
_mpi_delete(void *xport)
{
  _mpi_xport_t *mpi = xport;
  if (mpi->fini) {
    MPI_Finalize();
  }
  free(mpi);
}

static void
_mpi_pin(const void *base, size_t bytes, void *key)
{
}

static void
_mpi_unpin(const void *base, size_t bytes)
{
}

static void
_mpi_create_comm(const void *xport, void *c, void *active_ranks, int num_active,
                 int total)
{
  const _mpi_xport_t *mpi = xport;
  MPI_Comm *comm = (MPI_Comm *)c;
  MPI_Comm active_comm;
  MPI_Group active_group, world_group;
  if (num_active < total) {
    MPI_Comm_group(mpi->comm, &world_group);
    MPI_Group_incl(world_group, num_active, active_ranks, &active_group);
    MPI_Comm_create(mpi->comm, active_group, &active_comm);
    *comm = active_comm;
  } else {
    // in this case we dont have to create an active comm group
    // comm_group is MPI_COMM_WORKD
    *comm = mpi->comm;
  }
}

typedef struct {
  hpx_monoid_op_t op;
  int bytes;
  char operands[];
} val_t;

/// handle for reduction operation
/// MPI will call this function in a colelctive reduction op
/// and then delegate to the real action
void
op_handler(void *in, void *inout, int *len, MPI_Datatype *dp)
{
  val_t *v = (val_t *)in;
  val_t *out = (val_t *)inout;
  v->op(out->operands, v->operands, v->bytes);
}

static void
_mpi_allreduce(void *sendbuf, void *out, int count, void *datatype, void *op,
               void *c)
{
  MPI_Comm *comm = c;
  hpx_monoid_op_t *hpx_handle = (hpx_monoid_op_t *)op;
  int bytes = sizeof(val_t) + count;

  // prepare operands for function
  char *val = calloc(bytes, sizeof(char));
  char *result = calloc(bytes, sizeof(char));
  val_t *in = (val_t *)val;
  val_t *res = (val_t *)result;
  in->op = *hpx_handle;
  in->bytes = count;
  memcpy(in->operands, sendbuf, count);

  MPI_Op usrOp;
  // we assume this function is commutative for now, hence 1
  MPI_Op_create(op_handler, 1, &usrOp);

  MPI_Allreduce(val, result, bytes, MPI_BYTE, usrOp, *comm);
  memcpy(out, res->operands, count);

  MPI_Op_free( &usrOp );
  free(val);
  free(result);
}

static int
_mpi_iallreduce(void *sendbuf, void *out, int count, void *datatype, void *op,
               void *c, void *r)
{

  MPI_Comm *comm = c;
  MPI_Op operation = MPI_SUM;
  MPI_Datatype dt = MPI_INT;
  MPI_Request *req = (MPI_Request*) r;

  if(!op){
    operation = *((MPI_Op*)op);
  }
  if(!datatype){
    dt = *((MPI_Datatype*) datatype);
  }

  int e = MPI_Iallreduce(sendbuf, out, count, dt, operation, *comm, req);
  if (MPI_SUCCESS != e) {
    return log_error("failed MPI_Iallreduce: with count %d \n", count);
  }

  log_net("started MPI_Iallreduce with count to %d\n", count);
  return LIBHPX_OK;
}

isir_xport_t *
isir_xport_new_mpi(const config_t *cfg, gas_t *gas) {
  _mpi_xport_t *mpi = malloc(sizeof(*mpi));
  mpi->vtable.type           = HPX_TRANSPORT_MPI;
  mpi->vtable.delete         = _mpi_delete;
  mpi->vtable.check_tag      = _mpi_check_tag;
  mpi->vtable.sizeof_request = _mpi_sizeof_request;
  mpi->vtable.sizeof_status  = _mpi_sizeof_status;
  mpi->vtable.isend          = _mpi_isend;
  mpi->vtable.irecv          = _mpi_irecv;
  mpi->vtable.iprobe         = _mpi_iprobe;
  mpi->vtable.testsome       = _mpi_testsome;
  mpi->vtable.clear          = _mpi_clear;
  mpi->vtable.cancel         = _mpi_cancel;
  mpi->vtable.finish         = _mpi_finish;
  mpi->vtable.pin            = _mpi_pin;
  mpi->vtable.unpin          = _mpi_unpin;
  mpi->vtable.create_comm    = _mpi_create_comm;
  mpi->vtable.allreduce      = _mpi_allreduce;
  mpi->vtable.iallreduce     = _mpi_iallreduce;

  int already_initialized = 0;
  if (MPI_SUCCESS != MPI_Initialized(&already_initialized)) {
    log_error("mpi initialization failed\n");
    free(mpi);
    return NULL;
  }

  if (!already_initialized) {
    int level = MPI_THREAD_SINGLE;
    if (MPI_SUCCESS != MPI_Init_thread(NULL, NULL, LIBHPX_THREAD_LEVEL, &level)) {
      log_error("mpi initialization failed\n");
      free(mpi);
      return NULL;
    }

    if (level != LIBHPX_THREAD_LEVEL) {
      log_error("MPI thread level failed requested %d, received %d.\n",
                LIBHPX_THREAD_LEVEL, level);
      free(mpi);
      return NULL;
    }
  }

  // remember if we need to finalize MPI and use a duplication COMM_WORLD
  mpi->fini = !already_initialized;
  if (MPI_SUCCESS != MPI_Comm_dup(MPI_COMM_WORLD, &mpi->comm)) {
    log_error("mpi communicator duplication failed\n");
    free(mpi);
    return NULL;
  }

  log_trans("thread_support_provided = %d\n", LIBHPX_THREAD_LEVEL);
  return &mpi->vtable;
}
