// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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

#include <libhpx/config.h>
#include <libhpx/boot.h>
#include <libhpx/libhpx.h>
#include <libhpx/memory.h>
#include <libhpx/parcel.h>
#include "commands.h"
#include "parcel_emulation.h"
#include "xport.h"

typedef struct {
  size_t   n;
  size_t   i;
  char *base;
  char   key[];
} buffer_t;

typedef struct {
  void *addr;
  char   key[];
} remote_t;

typedef struct {
  parcel_emulator_t vtable;
  int                 rank;
  int                ranks;
  buffer_t           *recv;
  buffer_t           *send;
  remote_t        *remotes;
} reload_t;

static void _buffer_init(buffer_t *b, size_t n, pwc_xport_t *xport) {
  b->n = n;
  b->base = registered_calloc(1, n);
  int e = xport->key_find(xport, b->base, n, &b->key);
  dbg_check(e, "no key for newly allocated buffer (%p, %zu)\n", b->base, n);
}

static int _buffer_reload(buffer_t *send, pwc_xport_t *xport, xport_op_t *op) {
  dbg_error("reload unimplemented.\n");
  return LIBHPX_RETRY;
}

static int _buffer_send(buffer_t *send, pwc_xport_t *xport, xport_op_t *op) {
  size_t i = send->i;
  send->i += op->n;
  if (send->i >= send->n) {
    return _buffer_reload(send, xport, op);
  }

  xport->key_copy(&op->dest_key, &send->key);
  op->dest = send->base + i;
  return xport->pwc(op);
}

static int _reload_send(void *obj, pwc_xport_t *xport, int rank,
                        const hpx_parcel_t *p) {
  size_t n = parcel_size(p);
  xport_op_t op = {
    .rank = rank,
    .n = n,
    .dest = NULL,
    .dest_key = NULL,
    .src = p,
    .src_key = xport->key_find_ref(xport, p, n),
    .lop = command_pack(release_parcel, (uintptr_t)p),
    .rop = command_pack(recv_parcel, n)
  };

  if (!op.src_key) {
    dbg_error("no rdma key for local parcel (%p, %zu)\n", p, n);
  }

  reload_t *reload = obj;
  buffer_t *send = &reload->send[rank];
  return _buffer_send(send, xport, &op);
}

static hpx_parcel_t *_buffer_recv(buffer_t *recv) {
  const hpx_parcel_t *p = (void*)(recv->base + recv->i);
  recv->i += parcel_size(p);
  if (recv->i >= recv->n) {
    dbg_error("reload buffer recv overload\n");
  }
  // todo: don't do this, reference count the buffer
  hpx_parcel_t *clone = hpx_parcel_acquire(NULL, parcel_payload_size(p));
  memcpy(clone, p, parcel_size(p));
  return clone;
}

static hpx_parcel_t *_reload_recv(void *obj, int rank) {
  reload_t *reload = obj;
  buffer_t *recv = &reload->recv[rank];
  return _buffer_recv(recv);
}

static void _reload_delete(void *obj) {
  if (obj) {
    reload_t *reload = obj;
    local_free(reload->recv);
    registered_free(reload->send);
    local_free(reload->remotes);
    free(reload);
  }
}

void *parcel_emulator_new_reload(const config_t *cfg, boot_t *boot,
                                 pwc_xport_t *xport) {
  int rank = boot_rank(boot);
  int ranks = boot_n_ranks(boot);

  // Allocate the buffer.
  reload_t *reload = local_calloc(1, sizeof(*reload));
  reload->vtable.delete = _reload_delete;
  reload->vtable.send = _reload_send;
  reload->vtable.recv = _reload_recv;
  reload->rank = rank;
  reload->ranks = ranks;

  // Compute the size of some dynamically sized stuff.
  size_t key_size = xport->key_size();
  size_t buffer_size = sizeof(buffer_t) + key_size;
  size_t buffer_row_size = ranks * buffer_size;
  size_t remote_size = sizeof(remote_t) + key_size;
  size_t remote_table_size = ranks * remote_size;

  // Allocate my buffers (send is written via rdma, so it is registered).
  reload->recv = local_malloc(buffer_row_size);
  reload->send = registered_malloc(buffer_row_size);
  reload->remotes = local_malloc(remote_table_size);

  // Initialize the recv buffers for this rank.
  for (int i = 0, e = ranks; i < e; ++i) {
    _buffer_init(&reload->recv[i], cfg->pwc_parcelbuffersize, xport);
  }

  // Initialize a temporary array of remote pointers for this rank's sends.
  char key[key_size];
  int e = xport->key_find(xport, reload->send, buffer_row_size, &key);
  dbg_check(e, "no key for send (%p, %zu)\n", reload->send, buffer_row_size);

  remote_t *sends = local_malloc(remote_table_size);
  for (int i = 0, e = ranks; i < e; ++i) {
    sends[i].addr = &reload->send[i];
    xport->key_copy(&sends[i].key, &key);
  }

  // exchange all of the recv buffers, and all of the remote send pointers
  boot_alltoall(boot, reload->send, reload->recv, buffer_size, buffer_size);
  boot_alltoall(boot, reload->remotes, sends, remote_size, remote_size);

  // free the temporary array of remote pointers
  local_free(sends);

  // Now reload contains:
  //
  // 1) A row of initialized recv buffers, one for each sender.
  // 2) A row of initialized send buffers, one for each target (corresponding to
  //    their recv buffer for me).
  // 3) A table of remote pointers, one for each send buffer targeting me.
  return reload;
}
