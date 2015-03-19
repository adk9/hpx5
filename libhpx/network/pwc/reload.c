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
#include "parcel_emulation.h"
#include "xport.h"

typedef struct {
  size_t   n;
  size_t   i;
  void *base;
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
  pwc_xport_t       *xport;
  buffer_t           *recv;
  buffer_t           *send;
  remote_t        *remotes;
} reload_t;

static void _reload_delete(void *obj) {
  if (obj) {
    local_free(obj);
  }
}

static void _buffer_init(buffer_t *b, size_t n, pwc_xport_t *xport) {
  b->n = n;
  b->base = registered_calloc(1, n);
  int e = xport->key_find(xport, b->base, n, &b->key);
  dbg_check(e, "no key for newly allocated buffer (%p, %zu)\n", b->base, n);
}

static int _buffer_reload(buffer_t *b, xport_op_t *op, pwc_xport_t *xport) {
  return LIBHPX_RETRY;
}

static int _buffer_send(buffer_t *b, xport_op_t *op, pwc_xport_t *xport) {
  size_t i = b->i;
  b->i += op->n;
  if (b->i >= b->n) {
    return _buffer_reload(b, op, xport);
  }

  xport->key_copy(&op->dest_key, &b->key);
  op->dest = (char*)b->base + i;
  return xport->pwc(op);
}

static int _reload_send(void *obj, xport_op_t *op, hpx_parcel_t *p) {
  reload_t *reload = obj;
  pwc_xport_t *xport = reload->xport;
  op->n = parcel_size(p);
  op->src = p;
  int e = xport->key_find(xport, op->src, op->n, &op->src_key);
  dbg_check(e, "no rdma key for local parcel (%p, %zu)\n", op->src, op->n);
  buffer_t *buffer = &reload->send[op->rank];
  return _buffer_send(buffer, op, xport);
}

void *parcel_emulator_new_reload(const config_t *cfg, boot_t *boot,
                                 pwc_xport_t *xport) {
  int rank = boot_rank(boot);
  int ranks = boot_n_ranks(boot);

  // Allocate the buffer.
  reload_t *reload = local_calloc(1, sizeof(*reload));
  reload->vtable.delete = _reload_delete;
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
