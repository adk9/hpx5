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
  dbg_log_trans("photon: barrier unsupported.\n");
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
static int
_pin(transport_class_t *transport, const void* buffer, size_t len)
{
  void *b = (void*)buffer;
  if (photon_register_buffer(b, len))
    dbg_error("photon: could not pin buffer of size %lu.\n", len);
  return 1;
}


/// ----------------------------------------------------------------------------
/// Pinning necessary.
/// ----------------------------------------------------------------------------
static void
_unpin(transport_class_t *transport, const void* buffer, size_t len)
{
  void *b = (void*)buffer;
  if (photon_unregister_buffer(b, len))
    dbg_error("photon: could not unpin buffer %p of size %lu.\n", buffer, len);
}


/// ----------------------------------------------------------------------------
/// Put data via Photon
/// ----------------------------------------------------------------------------
static int
_put(transport_class_t *t, int dest, const void *data, size_t n, void *rbuffer,
     size_t rn, void *rid, void *r)
{
  int rc, flags;
  photon_t *photon = (photon_t *)t;
  void *b = (void*)data;
  struct photon_buffer_priv_t priv;
  struct photon_buffer_t pbuf;

  // If we are doing HW GAS, then we need to be in photon UD mode with special
  // mcast addressing.  This mode currently doesn't allow custom request IDs.
  if (photon->cfg.ibv.use_ud) {
    uint64_t saddr = block_id_ipv4mc(dest);
    photon_addr daddr = {.blkaddr.blk3 = saddr};
    int e = photon_send(&daddr, b, n, 0, r);
    if (e != PHOTON_OK)
      return dbg_error("photon: could not put %lu bytes to %i.\n", n, dest);
  }
  else {
    rc = photon_get_buffer_private(rbuffer, rn, &priv);
    if (rc != PHOTON_OK) {
      return dbg_error("photon: could not get buffer metadata for put: 0x%016lx (%lu).\n",
               (uintptr_t)rbuffer, rn);
    }

    pbuf.addr = (uintptr_t)rbuffer;
    pbuf.size = rn;
    pbuf.priv = priv;

    if (rid) {
      flags = PHOTON_REQ_USERID;
      *(photon_rid*)r = *(photon_rid*)rid;
    }
    else {
      flags = PHOTON_REQ_NIL;
    }

    rc = photon_post_os_put_direct(dest, b, n, &pbuf, flags, r);
    if (rc != PHOTON_OK) {
      return dbg_error("photon: could not complete put operation: 0x%016lx (%lu).\n",
               (uintptr_t)rbuffer, rn);
    }
  }

  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Get data via Photon
/// ----------------------------------------------------------------------------
static int
_get(transport_class_t *t, int dest, void *buffer, size_t n, const void *rdata,
     size_t rn, void *rid, void *r)
{
  int rc, flags;
  photon_t *photon = (photon_t*)t;
  void *b = (void*)rdata;
  struct photon_buffer_priv_t priv;
  struct photon_buffer_t pbuf;

  // Behavior of HW GAS and UD mode is undefined for a "get", this will most
  // likely need to be a signal to the target to "put" the data, and then wait.
  if (photon->cfg.ibv.use_ud) {
    photon_rid *id = (photon_rid*)r;
    int e = photon_recv(*id, buffer, n, 0);
    if (e != PHOTON_OK) {
      return dbg_error("photon: could not get from %i.\n", dest);
    }
  }
  else {
    rc = photon_get_buffer_private(b, rn, &priv);
    if (rc != PHOTON_OK) {
      return dbg_error("photon: could not get buffer metadata for get: 0x%016lx (%lu).\n",
               (uintptr_t)b, rn);
    }

    pbuf.addr = (uintptr_t)b;
    pbuf.size = rn;
    pbuf.priv = priv;

    if (rid) {
      flags = PHOTON_REQ_USERID;
      *(photon_rid*)r = *(photon_rid*)rid;
    }
    else {
      flags = PHOTON_REQ_NIL;
    }

    rc = photon_post_os_get_direct(dest, buffer, n, &pbuf, flags, r);
    if (rc != PHOTON_OK) {
      return dbg_error("photon: could not complete get operation: 0x%016lx (%lu).\n",
               (uintptr_t)b, rn);
    }
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
  void *b = (void*)data;

  //int e = photon_send(&daddr, b, n, 0, r);
  int e = photon_post_send_buffer_rdma(dest, b, n, PHOTON_DEFAULT_TAG, r);
  if (e != PHOTON_OK)
    return dbg_error("photon: could not send %lu bytes to %i.\n", n, dest);
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
    dbg_error("photon: probe failed.\n");
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
  int e = photon_wait_send_buffer_rdma(src, n, PHOTON_DEFAULT_TAG, r);
  if (e != PHOTON_OK) {
    return dbg_error("error in wait_send_buffer for %i\n", src);
  }

  // get the remote buffer
  e = photon_post_os_get(*(photon_rid*)r, src, buffer, n, PHOTON_DEFAULT_TAG, 0);
  if (e != PHOTON_OK)
    return dbg_error("could not receive %lu bytes from %i\n", n, src);

  return HPX_SUCCESS;
}


static int
_test(transport_class_t *t, void *request, int *success)
{
  int type = 0;
  struct photon_status_t status;
  photon_rid *id = (photon_rid*)request;
  int e = photon_test(*id, success, &type, &status);
  if (e < 0)
    return dbg_error("photon: failed photon_test.\n");

  // send back the FIN message for local EVQUEUE completions (type==0)
  if ((*success == 1) && (type == 0)) {
    e = photon_send_FIN(*id, status.src_addr.global.proc_id, 0);
    if (e != PHOTON_OK) {
      return dbg_error("photon: could not send FIN back to %lu.\n",
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
  if (flush)
    network_progress_flush(photon->progress);
}


static uint32_t _photon_get_send_limit(void) {
  return UINT32_MAX;
}

static uint32_t _photon_get_recv_limit(void) {
  return UINT32_MAX;
}



transport_class_t *transport_new_photon(void) {
  photon_t *photon = malloc(sizeof(*photon));
  photon->class.type           = HPX_TRANSPORT_PHOTON;
  photon->class.id             = _id;
  photon->class.barrier        = _barrier;
  photon->class.request_size   = _request_size;
  photon->class.request_cancel = _request_cancel;
  photon->class.adjust_size    = _adjust_size;
  photon->class.get_send_limit = _photon_get_send_limit;
  photon->class.get_recv_limit = _photon_get_recv_limit;

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

  // runtime configuration options
  char* eth_dev;
  char* ib_dev;
  char* backend;
  int ib_port;
  int use_cma;
  int val = 0;

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
  cfg->meta_exch       = PHOTON_EXCH_EXTERNAL;
  cfg->nproc           = here->ranks;
  cfg->address         = here->rank;
  cfg->comm            = NULL;
  cfg->forwarder.use_forwarder   = 0;
  cfg->ibv.use_cma         = use_cma;
  cfg->ibv.use_ud          = 0;  // don't enable this unless we're doing HW GAS
  cfg->ibv.ud_gid_prefix   = "ff0e::ffff:0000:0000";
  cfg->ibv.eth_dev         = eth_dev;
  cfg->ibv.ib_dev          = ib_dev;
  cfg->ibv.ib_port         = ib_port;
  cfg->cap.eager_buf_size  = -1;  // default 128k
  cfg->cap.small_msg_size  = -1;  // default 8192
  cfg->cap.small_pwc_size  =  0;  // 0 disabled
  cfg->exch.allgather      = (typeof(cfg->exch.allgather))here->boot->allgather;
  cfg->exch.barrier        = (typeof(cfg->exch.barrier))here->boot->barrier;
  cfg->backend             = backend;

  val = photon_initialized();
  if (!val) {
    if (photon_init(cfg) != PHOTON_OK) {
      dbg_error("photon: failed to initialize transport.\n");
      return NULL;
    };
  }

  photon->progress     = network_progress_new(&photon->class);
  if (!photon->progress)
    dbg_error("photon: failed to start the progress loop.\n");

  return &photon->class;
}
