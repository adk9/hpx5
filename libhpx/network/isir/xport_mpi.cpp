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
  const _mpi_xport_t *mpi = static_cast<const _mpi_xport_t *>(xport);
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

static size_t
_mpi_sizeof_comm(void) {
  return sizeof(MPI_Comm);
}

static int
_mpi_isend(const void *xport, int to, const void *from, unsigned n, int tag,
           void *req)
{
  const _mpi_xport_t *mpi = static_cast<const _mpi_xport_t *>(xport);
  MPI_Request *r = static_cast<MPI_Request*>(req);
  int e = MPI_Isend((void *)from, n, MPI_BYTE, to, tag, mpi->comm, r);
  if (MPI_SUCCESS != e) {
    return log_error("failed MPI_Isend: %u bytes to %d\n", n, to);
  }

  log_net("started MPI_Isend: %u bytes to %d\n", n, to);
  return LIBHPX_OK;
}

static int
_mpi_irecv(const void *xport, void *to, size_t n, int tag, void *request) {
  const _mpi_xport_t *mpi = static_cast<const _mpi_xport_t *>(xport);
  const int src = MPI_ANY_SOURCE;
  const MPI_Comm com = mpi->comm;
  MPI_Request *r = static_cast<MPI_Request*>(request);
  if (MPI_SUCCESS != MPI_Irecv(to, n, MPI_BYTE, src, tag, com, r)) {
    return log_error("could not start irecv\n");
  }
  return LIBHPX_OK;
}

static int
_mpi_iprobe(const void *xport, int *tag) {
  const _mpi_xport_t *mpi = static_cast<const _mpi_xport_t *>(xport);
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
_mpi_testsome(int n, void *requests, int *nout, int *out, void *statuses)
{
  auto reqs = static_cast<MPI_Request*>(requests);
  auto stats = static_cast<MPI_Status*>(statuses);
  if (!stats) {
    stats = MPI_STATUS_IGNORE;
  }

  int e = MPI_Testsome(n, reqs, nout, out, stats); (void)e;
  dbg_assert_str(e == MPI_SUCCESS, "MPI_Testsome error is fatal.\n");
  dbg_assert_str(*nout != MPI_UNDEFINED, "silent MPI_Testsome() error.\n");
}

static void
_mpi_clear(void *request)
{
  MPI_Request *r = static_cast<MPI_Request*>(request);
  *r = MPI_REQUEST_NULL;
}

static int
_mpi_cancel(void *request, int *cancelled)
{
  auto r = static_cast<MPI_Request*>(request);
  if (*r == MPI_REQUEST_NULL) {
    *cancelled = 1;
    return LIBHPX_OK;
  }

  if (MPI_SUCCESS != MPI_Cancel(r)) {
    return log_error("could not cancel MPI request\n");
  }

  MPI_Status status;
  if (MPI_SUCCESS != MPI_Wait(r, &status)) {
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
  auto s = static_cast<MPI_Status*>(status);
  if (MPI_SUCCESS != MPI_Get_count(s, MPI_BYTE, bytes)) {
    dbg_error("could not extract the size of an irecv\n");
  }

  dbg_assert(*bytes > 0);
  *src = s->MPI_SOURCE;
}

static void
_mpi_deallocate(void *xport)
{
  auto mpi = static_cast<_mpi_xport_t *>(xport);
  if (mpi->fini) {
    MPI_Finalize();
  }
  delete mpi;
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
  auto mpi = static_cast<const _mpi_xport_t*>(xport);
  auto comm = static_cast<MPI_Comm*>(c);
  MPI_Comm active_comm;
  MPI_Group active_group, world_group;
  if (num_active < total) {
    MPI_Comm_group(mpi->comm, &world_group);
    MPI_Group_incl(world_group, num_active, static_cast<const int*>(active_ranks), &active_group);
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
  auto comm = static_cast<MPI_Comm*>(c);
  auto hpx_handle = static_cast<hpx_monoid_op_t*>(op);
  int bytes = sizeof(val_t) + count;

  // prepare operands for function
  void    *val = calloc(bytes, sizeof(char));
  void *result = calloc(bytes, sizeof(char));
  val_t    *in = static_cast<val_t*>(val);
  val_t   *res = static_cast<val_t*>(result);
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

isir_xport_t *
isir_xport_new_mpi(const config_t *cfg, gas_t *gas, void *comm) {
  auto mpi = new _mpi_xport_t;
  mpi->vtable.type           = HPX_TRANSPORT_MPI;
  mpi->vtable.deallocate     = _mpi_deallocate;
  mpi->vtable.check_tag      = _mpi_check_tag;
  mpi->vtable.sizeof_request = _mpi_sizeof_request;
  mpi->vtable.sizeof_status  = _mpi_sizeof_status;
  mpi->vtable.sizeof_comm    = _mpi_sizeof_comm;
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

  // remember if we need to finalize MPI and use a duplication COMM_WORLD
  mpi->fini = 0;
  mpi->comm = *(MPI_Comm*)comm;
  log_trans("thread_support_provided = %d\n", LIBHPX_THREAD_LEVEL);
  return &mpi->vtable;
}
