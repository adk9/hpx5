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
#include <errno.h>
#include <error.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>                            // required for jemalloc
#include <jemalloc/jemalloc.h>
#include <sys/mman.h>
#include <photon.h>

#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/transport.h"
#include "libhpx/routing.h"
#include "progress.h"


#define PHOTON_DEFAULT_TAG 13

static char* photon_default_eth_dev = "roce0";
static char* photon_default_ib_dev = "qib0";
static int   photon_default_ib_port = 1;
static char* photon_default_backend = "verbs";

/// the Photon transport
typedef struct {
  transport_class_t     class;
  struct photon_config_t  cfg;
  progress_t        *progress;
  unsigned              arena;
  chunk_alloc_t        *alloc;
  chunk_dalloc_t      *dalloc;
} photon_t;


/// ----------------------------------------------------------------------------
/// Get the ID for a Photon transport.
/// ----------------------------------------------------------------------------
static const char *
_id(void)
{
  return "Photon";
}


/// ----------------------------------------------------------------------------
/// Photon barrier.
/// ----------------------------------------------------------------------------
static void
_barrier(void)
{
  dbg_log("photon: barrier unsupported.\n");
}


/// ----------------------------------------------------------------------------
/// Return the size of a Photon request.
/// ----------------------------------------------------------------------------
static int
_request_size(void)
{
  return sizeof(uint64_t);
}


static int
_adjust_size(int size)
{
  return size;
}


/// ----------------------------------------------------------------------------
/// Cancel an active Photon request.
/// ----------------------------------------------------------------------------
static int
_request_cancel(void *request)
{
  // Is it possible to cancel an active request/ledger?
  return 0;
}


/// ----------------------------------------------------------------------------
/// Shut down Photon, and delete the transport.
/// ----------------------------------------------------------------------------
static void
_delete(transport_class_t *transport)
{
  photon_t *photon = (photon_t*)transport;
  network_progress_delete(photon->progress);
  photon_finalize();
  free(transport);
}


/// ----------------------------------------------------------------------------
/// Pinning necessary.
/// ----------------------------------------------------------------------------
static void
_pin(transport_class_t *transport, const void* buffer, size_t len)
{
  void *b = (void*)buffer;
  if (photon_register_buffer(b, len))
    dbg_error("Could not pin buffer of size %lu for photon\n", len);
}


/// ----------------------------------------------------------------------------
/// Pinning necessary.
/// ----------------------------------------------------------------------------
static void
_unpin(transport_class_t *transport, const void* buffer, size_t len)
{
  void *b = (void*)buffer;
  if (photon_unregister_buffer(b, len))
    dbg_error("Could not un-pin buffer %p of size %lu for photon\n", buffer, len);
}

/// ----------------------------------------------------------------------------
/// Put data via Photon
/// ----------------------------------------------------------------------------
static int
_put(transport_class_t *t, int dest, const void *data, size_t n, void *rbuffer,
     size_t rn, void *r)
{
  int rc;
  void *b = (void*)data;
  struct photon_buffer_priv_t priv;
  struct photon_buffer_t pbuf;

  rc = photon_get_buffer_private(rbuffer, rn, &priv);
  if (rc != PHOTON_OK) {
    return dbg_error("Could not get buffer metadata for put: 0x%016lx (%lu)\n",
                     (uintptr_t)rbuffer, rn);
  }

  pbuf.addr = (uintptr_t)rbuffer;
  pbuf.size = rn;
  pbuf.priv = priv;

  rc = photon_post_os_put_direct(dest, b, n, PHOTON_DEFAULT_TAG, &pbuf, r);
  if (rc != PHOTON_OK) {
    return dbg_error("Could not complete put operation: 0x%016lx (%lu)\n",
                     (uintptr_t)rbuffer, rn);
  }

  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Get data via Photon
/// ----------------------------------------------------------------------------
static int
_get(transport_class_t *t, int dest, void *buffer, size_t n, const void *rdata,
     size_t rn, void *r)
{
  int rc;
  void *b = (void*)rdata;
  struct photon_buffer_priv_t priv;
  struct photon_buffer_t pbuf;

  rc = photon_get_buffer_private(b, rn, &priv);
  if (rc != PHOTON_OK) {
    return dbg_error("Could not get buffer metadata for get: 0x%016lx (%lu)\n",
                     (uintptr_t)b, rn);
  }

  pbuf.addr = (uintptr_t)b;
  pbuf.size = rn;
  pbuf.priv = priv;

  rc = photon_post_os_get_direct(dest, buffer, n, PHOTON_DEFAULT_TAG, &pbuf, r);
  if (rc != PHOTON_OK) {
    return dbg_error("Could not complete get operation: 0x%016lx (%lu)\n",
                     (uintptr_t)b, rn);
  }

  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Send data via Photon.
///
/// Presumably this will be an "eager" send. Don't use "data" until it's done!
/// ----------------------------------------------------------------------------
static int
_send(transport_class_t *t, int dest, const void *data, size_t n, void *r)
{
  //uint64_t saddr = block_id_ipv4mc(dest);
  //photon_t *photon = (photon_t*)t;
  //photon_addr daddr = {.blkaddr.blk3 = saddr};
  void *b = (void*)data;

  //int e = photon_send(&daddr, b, n, 0, r);
  int e = photon_post_send_buffer_rdma(dest, b, n, PHOTON_DEFAULT_TAG, r);
  if (e != PHOTON_OK)
    return dbg_error("Photon could not send %lu bytes to %i.\n", n, dest);
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Probe Photon ledger to see if anything has been received.
/// ----------------------------------------------------------------------------
static size_t
_probe(transport_class_t *transport, int *source)
{
  int photon_src = *source;
  int flag = 0;
  struct photon_status_t status;
  // receive from anyone. is this correct?
  //photon_addr addr = {.s_addr = 0};
  //int e = photon_probe(&addr, &flag, &status);

  if (*source == TRANSPORT_ANY_SOURCE) {
    photon_src = PHOTON_ANY_SOURCE;
  }

  int e = photon_probe_ledger(photon_src, &flag, PHOTON_SEND_LEDGER, &status);
  if (e < 0) {
    dbg_error("photon probe failed.\n");
    return 0;
  }

  if (flag) {
    // update the source to the actual source, and return the number of bytes
    // available
    *source = (int)status.src_addr.global.proc_id;
    return status.size;
  }
  else {
    return 0;
  }
}


/// ----------------------------------------------------------------------------
/// Receive a buffer.
/// ----------------------------------------------------------------------------
static int
_recv(transport_class_t *t, int src, void* buffer, size_t n, void *r)
{
  //photon_t *photon = (photon_t*)t;
  //uint64_t *id = (uint64_t*)r;
  //int e = photon_recv(*id, buffer, n, 0);
  // make sure we have remote buffer metadata
  int e = photon_wait_send_buffer_rdma(src, PHOTON_DEFAULT_TAG, r);
  if (e != PHOTON_OK) {
    return dbg_error("error in wait_send_buffer for %i\n", src);
  }

  // get the remote buffer
  e = photon_post_os_get(*(uint32_t*)r, src, buffer, n, PHOTON_DEFAULT_TAG, 0);
  if (e != PHOTON_OK)
    return dbg_error("could not receive %lu bytes from %i\n", n, src);

  return HPX_SUCCESS;
}


static int
_test(transport_class_t *t, void *request, int *success)
{
  int type = 0;
  struct photon_status_t status;
  uint32_t *id = (uint32_t*)request;
  int e = photon_test(*id, success, &type, &status);
  if (e < 0)
    return dbg_error("failed photon_test.\n");

  // send back the FIN message for local EVQUEUE completions (type==0)
  if ((*success == 1) && (type == 0)) {
    e = photon_send_FIN(*id, status.src_addr.global.proc_id);
    if (e != PHOTON_OK) {
      return dbg_error("could not send FIN back to %lu\n",
                       status.src_addr.global.proc_id);
    }
  }

  return HPX_SUCCESS;
}


static void
_progress(transport_class_t *t, bool flush)
{
  photon_t *photon = (photon_t*)t;
  network_progress_poll(photon->progress);
  //if (flush)
  //  network_progress_flush(photon->progress);
}


static void *
_malloc(transport_class_t *t, size_t bytes, size_t align)
{
  photon_t *photon = (photon_t *)t;
  void *p = mallocx(bytes,
                    MALLOCX_ALIGN(align) | MALLOCX_ARENA(photon->arena));
  if (!p)
    dbg_log("failed network allocation.\n");

  return p;
}


static void
_free(transport_class_t *t, void *p)
{
  photon_t *photon = (photon_t *)t;
  dallocx(p, MALLOCX_ARENA(photon->arena));
}


static void *
_alloc_pinned_chunk(size_t size, size_t alignment, bool *zero, unsigned arena_ind)
{
  photon_t *t = (photon_t *)here->transport;
  assert(t);
  void *chunk = t->alloc(size, alignment, zero, arena_ind);
  if (!chunk) {
    dbg_error("Photon transport could not alloc jemalloc chunk of size %lu\n", size);
    return NULL;
  }

  if ((uintptr_t)chunk % alignment != 0) {
    dbg_error("Photon transport could not alloc jemalloc chunk of size %lu "
              "with alignment %lu\n", size, alignment);
    hpx_abort();
  }

  if (photon_register_buffer(chunk, size)) {
    dbg_error("Photon transport could not pin buffer %p of size %lu\n", chunk, size);
    t->dalloc(chunk, size, arena_ind);
  }

  return chunk;
}


static bool
_dalloc_pinned_chunk(void *chunk, size_t size, unsigned arena_ind)
{
  if (photon_unregister_buffer(chunk, size))
   dbg_error("Photon transport could not un-pin buffer %p of size %lu\n",
             chunk, size);

  photon_t *t = (photon_t *)here->transport;
  assert(t);
  return t->dalloc(chunk, size, arena_ind);
}


transport_class_t *transport_new_photon(void) {
  photon_t *photon = malloc(sizeof(*photon));
  photon->class.type           = HPX_TRANSPORT_PHOTON;
  photon->class.id             = _id;
  photon->class.barrier        = _barrier;
  photon->class.request_size   = _request_size;
  photon->class.request_cancel = _request_cancel;
  photon->class.adjust_size    = _adjust_size;

  photon->class.delete         = _delete;
  photon->class.pin            = _pin;
  photon->class.unpin          = _unpin;
  photon->class.put            = _put;
  photon->class.get            = _get;
  photon->class.send           = _send;
  photon->class.probe          = _probe;
  photon->class.recv           = _recv;
  photon->class.test           = _test;
  photon->class.testsome       = NULL;
  photon->class.progress       = _progress;
  photon->class.malloc         = _malloc;
  photon->class.free           = _free;

  // runtime configuration options
  char* eth_dev;
  char* ib_dev;
  char* backend;
  int ib_port;
  int use_cma;

  int val = 0;
  MPI_Initialized(&val);
  if (!val) {
    dbg_error("photon transport only supports mpi bootstrap.\n");
    return NULL;
  }

  // TODO: make eth_dev and ib_dev runtime configurable!
  eth_dev = getenv("HPX_USE_ETH_DEV");
  ib_dev = getenv("HPX_USE_IB_DEV");
  backend = getenv("HPX_USE_BACKEND");

  if (eth_dev == NULL)
    eth_dev = photon_default_eth_dev;
  if (ib_dev == NULL)
    ib_dev = photon_default_ib_dev;
  if (backend == NULL)
    backend = photon_default_backend;
  if(getenv("HPX_USE_CMA") == NULL)
    use_cma = 1;
  else
    use_cma = atoi(getenv("HPX_USE_CMA"));
  if (getenv("HPX_USE_IB_PORT") == NULL)
    ib_port = photon_default_ib_port;
  else
    ib_port = atoi(getenv("HPX_USE_IB_PORT"));

  struct photon_config_t *cfg = &photon->cfg;
  cfg->meta_exch       = PHOTON_EXCH_MPI;
  cfg->nproc           = here->ranks;
  cfg->address         = here->rank;
  cfg->comm            = MPI_COMM_WORLD;
  cfg->use_forwarder   = 0;
  cfg->use_cma         = use_cma;
  cfg->use_ud          = 0;
  cfg->ud_gid_prefix   = "ff0e::ffff:0000:0000";
  cfg->eth_dev         = eth_dev;
  cfg->ib_dev          = ib_dev;
  cfg->ib_port         = ib_port;
  cfg->backend         = backend;

  val = photon_initialized();
  if (!val) {
    if (photon_init(cfg) != PHOTON_OK) {
      dbg_error("failed to initialize photon transport.\n");
      return NULL;
    };
  }

  photon->progress     = network_progress_new();
  if (!photon->progress) {
    dbg_error("failed to start the transport progress loop.\n");
    hpx_abort();
  }

  size_t sz = sizeof(photon->arena);
  int error = mallctl("arenas.extend", &photon->arena, &sz, NULL, 0);
  if (error) {
    dbg_error("failed to allocate a pinned arena %d.\n", error);
    hpx_abort();
  }

  sz = sizeof(photon->alloc);
  char path[128];
  snprintf(path, 128, "arena.%u.chunk.alloc", photon->arena);
  chunk_alloc_t *alloc = _alloc_pinned_chunk;
  error = mallctl(path, (void*)&photon->alloc, &sz, (void*)&alloc,
                  sizeof(alloc));
  if (error) {
    dbg_error("Photon failed to set arena allocator\n");
    hpx_abort();
  }

  sz = sizeof(photon->dalloc);
  snprintf(path, 128, "arena.%u.chunk.dalloc", photon->arena);
  chunk_dalloc_t *dalloc = _dalloc_pinned_chunk;
  error = mallctl(path, (void*)&photon->dalloc, &sz, (void*)&dalloc,
                  sizeof(dalloc));
  if (error) {
    dbg_error("Photon failed to set arena de-allocator.\n");
    hpx_abort();
  }

  return &photon->class;
}
