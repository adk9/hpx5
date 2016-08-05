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

#include "parcel_emulation.h"
#include "Commands.h"
#include "xport.h"
#include "PWCNetwork.h"
#include <libhpx/action.h>
#include <libhpx/boot.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/parcel.h>
#include <libhpx/parcel_block.h>
#include <libhpx/scheduler.h>
#include <string.h>
#include <stdlib.h>

namespace {
using namespace libhpx::network::pwc;

struct Buffer {
  size_t              n;
  size_t              i;
  parcel_block_t *block;
  xport_key_t       key;
};

struct Remote {
  void      *addr;
  xport_key_t key;
};

struct Reload {
  parcel_emulator_t vtable;
  unsigned            rank;
  unsigned           ranks;
  Buffer             *recv;
  xport_key_t     recv_key;
  Buffer             *send;
  xport_key_t     send_key;
  Remote        *remotes;
};
}

static void
_buffer_fini(Buffer *b)
{
  if (b) {
    parcel_block_delete(b->block);
  }
}

static void
_buffer_reload(Buffer *b, pwc_xport_t *xport)
{
  b->block = parcel_block_new(b->n, b->n, &b->i);
  xport->key_find(xport, b->block, b->n, &b->key);
}

static void
_buffer_init(Buffer *b, size_t n, pwc_xport_t *xport)
{
  b->n = n;
  _buffer_reload(b, xport);
}

void
Command::recvParcel(unsigned src) const
{
#ifdef __LP64__
  auto p = reinterpret_cast<hpx_parcel_t*>(arg_);
#else
  dbg_assert((arg_ & 0xffffffff) == arg_);
  auto p = reinterpret_cast<hpx_parcel_t*>((uintptr_t)arg_);
#endif
  p->src = src;
  parcel_set_state(p, PARCEL_SERIALIZED | PARCEL_BLOCK_ALLOCATED);
  EVENT_PARCEL_RECV(p->id, p->action, p->size, p->src, p->target);
  scheduler_spawn(p);
}

static int
_buffer_send(Buffer *send, pwc_xport_t *xport, xport_op_t *op)
{
  int i = send->i;
  dbg_assert(!(i & 7));
  size_t r = send->n - i;
  if (op->n < r) {
    // make sure i stays 8-byte aligned
    size_t align = ALIGN(op->n, 8);
    send->i += op->n + align;
    log_parcel("allocating %zu bytes in buffer %p (%zu remain)\n",
               op->n + align, (void*)send->block, send->n - send->i);
    op->dest_key = &send->key;
    op->dest = parcel_block_at(send->block, i);
    op->rop = Command::RecvParcel(static_cast<hpx_parcel_t*>(op->dest));
    return xport->pwc(op);
  }

  op->n = 0;
  op->src = nullptr;
  op->src_key = nullptr;
  op->lop = Command::Nop();
  op->rop = Command::ReloadRequest(r);
  int e = xport->cmd(op->rank, op->lop, op->rop);
  if (LIBHPX_OK == e) {
    return LIBHPX_RETRY;
  }

  dbg_error("could not complete send operation\n");
}

static int
_reload_send(void *obj, pwc_xport_t *xport, unsigned rank, const hpx_parcel_t *p)
{
  size_t n = parcel_size(p);
  xport_op_t op;
  op.rank = rank;
  op.n = n;
  op.dest = nullptr;
  op.dest_key = nullptr;
  op.src = p;
  op.src_key = xport->key_find_ref(xport, p, n);
  op.lop = Command::DeleteParcel(p);
  op.rop = Command::Nop();

  if (!op.src_key) {
    dbg_error("no rdma key for local parcel (%p, %zu)\n", (void*)p, n);
  }

  Reload *reload = static_cast<Reload*>(obj);
  Buffer *send = &reload->send[rank];
  return _buffer_send(send, xport, &op);
}

static hpx_parcel_t *
_buffer_recv(Buffer *recv)
{
  auto p = static_cast<const hpx_parcel_t*>(parcel_block_at(recv->block, recv->i));
  recv->i += parcel_size(p);
  if (recv->i >= recv->n) {
    dbg_error("reload buffer recv overload\n");
  }
  return parcel_clone(p);
}

static hpx_parcel_t *
_reload_recv(void *obj, unsigned rank)
{
  Reload *reload = static_cast<Reload*>(obj);
  Buffer *recv = &reload->recv[rank];
  return _buffer_recv(recv);
}

static void
_reload_deallocate(void *obj)
{
  if (obj) {
    Reload *reload = static_cast<Reload*>(obj);
    for (int i = 0, e = reload->ranks; i < e; ++i) {
      _buffer_fini(&reload->recv[i]);
    }
    registered_free(reload->recv);
    registered_free(reload->send);
    free(reload->remotes);
    free(reload);
  }
}

parcel_emulator_t *
libhpx::network::pwc::parcel_emulator_new_reload(const config_t *cfg,
                                                 boot_t *boot,
                                                 pwc_xport_t *xport)
{
  int rank = boot_rank(boot);
  int ranks = boot_n_ranks(boot);

  // Allocate the buffer.
  Reload *reload = static_cast<Reload*>(calloc(1, sizeof(*reload)));
  reload->vtable.deallocate = _reload_deallocate;
  reload->vtable.send = _reload_send;
  reload->vtable.recv = _reload_recv;
  reload->rank = rank;
  reload->ranks = ranks;

  // Allocate my buffers.
  size_t buffer_row_size = ranks * sizeof(Buffer);
  size_t remote_table_size = ranks * sizeof(Remote);
  reload->recv = static_cast<Buffer*>(registered_malloc(buffer_row_size));
  reload->send = static_cast<Buffer*>(registered_malloc(buffer_row_size));
  reload->remotes = static_cast<Remote*>(malloc(remote_table_size));

  // Grab the keys for the recv and send rows
  xport->key_find(xport, reload->send, buffer_row_size, &reload->send_key);
  xport->key_find(xport, reload->recv, buffer_row_size, &reload->recv_key);

  // Initialize the recv buffers for this rank.
  for (int i = 0, e = ranks; i < e; ++i) {
    Buffer *recv = &reload->recv[i];
    _buffer_init(recv, cfg->pwc_parcelbuffersize, xport);
  }

  // Initialize a temporary array of remote pointers for this rank's sends.
  Remote *remotes = static_cast<Remote*>(malloc(remote_table_size));
  for (int i = 0, e = ranks; i < e; ++i) {
    remotes[i].addr = &reload->send[i];
    xport->key_copy(&remotes[i].key, &reload->send_key);
  }

  // exchange all of the recv buffers, and all of the remote send pointers
  size_t buffer_size = sizeof(Buffer);
  size_t remote_size = sizeof(Remote);
  boot_alltoall(boot, reload->send, reload->recv, buffer_size, buffer_size);
  boot_alltoall(boot, reload->remotes, remotes, remote_size, remote_size);

  // free the temporary array of remote pointers
  free(remotes);

  // just do a sanity check to make sure the alltoalls worked
  if (DEBUG) {
    Buffer *send = &reload->send[rank];
    Buffer *recv = &reload->recv[rank];
    dbg_assert(send->n == recv->n);
    dbg_assert(send->i == recv->i);
    dbg_assert(send->block == recv->block);
    dbg_assert(!strncmp(send->key, recv->key, XPORT_KEY_SIZE));
    (void)send;
    (void)recv;
  }

  // Now reload contains:
  //
  // 1) A row of initialized recv buffers, one for each sender.
  // 2) A row of initialized send buffers, one for each target (corresponding to
  //    their recv buffer for me).
  // 3) A table of remote pointers, one for each send buffer targeting me.
  return &reload->vtable;
}

void
Command::reloadRequest(unsigned src) const {
  pwc_xport_t *xport = PWCNetwork::Instance().xport_;
  Reload *reload = reinterpret_cast<Reload*>(PWCNetwork::Instance().parcels_);
  Buffer *recv = &reload->recv[src];
  size_t n = arg_;
  if (n) {
    parcel_block_deduct(recv->block, n);
  }
  _buffer_reload(recv, xport);

  xport_op_t op;
  op.rank = src;
  op.n = sizeof(*recv);
  op.dest = reload->remotes[src].addr;
  op.dest_key = reload->remotes[src].key;
  op.src = recv;
  op.src_key = reload->recv_key;
  op.lop = Command::Nop();
  op.rop = Command::ReloadReply();

  dbg_check( xport->pwc(&op) );
}
