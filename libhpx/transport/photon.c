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
#include <photon.h>

#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/transport.h"
#include "libhpx/routing.h"
#include "progress.h"


/// the Photon transport
typedef struct {
  transport_class_t       class;
  struct photon_config_t  cfg;
  progress_t             *progress;
} photon_t;


/// ----------------------------------------------------------------------------
/// Get the ID for a Photon transport.
/// ----------------------------------------------------------------------------
static const char *_id(void) {
  return "Photon";
}


/// ----------------------------------------------------------------------------
/// Photon barrier.
/// ----------------------------------------------------------------------------
static void _barrier(void) {
  dbg_log("photon: barrier unsupported.\n");
}


/// ----------------------------------------------------------------------------
/// Return the size of a Photon request.
/// ----------------------------------------------------------------------------
static int _request_size(void) {
  return sizeof(uint64_t);
}


static int _adjust_size(int size) {
  return size;
}


/// ----------------------------------------------------------------------------
/// Cancel an active Photon request.
/// ----------------------------------------------------------------------------
static int _request_cancel(void *request) {
  // Is it possible to cancel an active request/ledger?
  return 0;
}


/// ----------------------------------------------------------------------------
/// Shut down Photon, and delete the transport.
/// ----------------------------------------------------------------------------
static void _delete(transport_class_t *transport) {
  photon_t *photon = (photon_t*)transport;
  network_progress_delete(photon->progress);
  photon_finalize();
  free(transport);
}


/// ----------------------------------------------------------------------------
/// Pinning not necessary.
/// ----------------------------------------------------------------------------
static void _pin(transport_class_t *transport, const void* buffer, size_t len) {
}


/// ----------------------------------------------------------------------------
/// Unpinning not necessary.
/// ----------------------------------------------------------------------------
static void _unpin(transport_class_t *transport, const void* buffer, size_t len) {
}


/// ----------------------------------------------------------------------------
/// Send data via Photon.
///
/// Presumably this will be an "eager" send. Don't use "data" until it's done!
/// ----------------------------------------------------------------------------
static int _send(transport_class_t *t, int dest, const void *data, size_t n, void *r)
{
  uint64_t saddr = block_id_ipv4mc(dest);
  photon_t *photon = (photon_t*)t;
  photon_addr daddr = {.blkaddr.blk3 = saddr};
  void *b = (void*)data;
  int e = photon_send(&daddr, b, n, 0, r);
  if (e != PHOTON_OK)
    return dbg_error("Photon could not send %lu bytes to %i.\n", n, dest);
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Probe MPI to see if anything has been received.
/// ----------------------------------------------------------------------------
static size_t _probe(transport_class_t *transport, int *source) {
  if (*source != TRANSPORT_ANY_SOURCE) {
    dbg_error("photon transport can not currently probe source %d\n", *source);
    return 0;
  }

  int flag = 0;
  struct photon_status_t status;
  // receive from anyone. is this correct?
  photon_addr addr = {.s_addr = 0};
  int e = photon_probe(&addr, &flag, &status);
  if (e < 0) {
    dbg_error("photon probe failed.\n");
    return 0;
  }

  if (flag) {
    // update the source to the actual source, and return the number of bytes
    // available
    // TODO: pass back the request ID for the subsequent photon_recv() (status.request)
    *source = status.src_addr.global.proc_id;
    return status.size;
  }
  else {
    return 0;
  }
}


/// ----------------------------------------------------------------------------
/// Receive a buffer.
/// ----------------------------------------------------------------------------
static int _recv(transport_class_t *t, int src, void* buffer, size_t n, void *r) {
  photon_t *photon = (photon_t*)t;
  uint64_t *id = (uint64_t*)r;

  int e = photon_recv(*id, buffer, n, 0);
  if (e != PHOTON_OK)
    return dbg_error("could not receive %lu bytes from %i", n, src);

  return HPX_SUCCESS;
}


static int _test(transport_class_t *t, void *request, int *success) {
  int type = 0;
  struct photon_status_t status;
  uint32_t *id = (uint32_t*)request;
  int e = photon_test(*id, success, &type, &status);
  if (e < 0)
    return dbg_error("failed photon_test.\n");

  return HPX_SUCCESS;
}

static void _progress(transport_class_t *t, bool flush) {
  photon_t *photon = (photon_t*)t;
  network_progress_poll(photon->progress);
  if (flush)
    network_progress_flush(photon->progress);
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
  photon->class.send           = _send;
  photon->class.probe          = _probe;
  photon->class.recv           = _recv;
  photon->class.test           = _test;
  photon->class.testsome       = NULL;
  photon->class.progress       = _progress;

  int val = 0;
  MPI_Initialized(&val);
  if (!val) {
    dbg_error("photon transport only supports mpi bootstrap.\n");
    return NULL;
  }

  struct photon_config_t *cfg = &photon->cfg;
  cfg->meta_exch       = PHOTON_EXCH_MPI;
  cfg->nproc           = here->ranks;
  cfg->address         = here->rank;
  cfg->comm            = MPI_COMM_WORLD;
  cfg->use_forwarder   = 0;
  cfg->use_cma         = 0;
  cfg->use_ud          = 1;
  cfg->ud_gid_prefix   = "ff0e::ffff:0000:0000";
  cfg->eth_dev         = "roce0";
  cfg->ib_dev          = "mlx4_0";
  cfg->ib_port         = 1;
  cfg->backend         = "verbs";

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
  return &photon->class;
}
