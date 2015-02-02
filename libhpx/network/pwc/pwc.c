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
# include "config.h"
#endif

#include <stdlib.h>

#include "libhpx/boot.h"
#include "libhpx/config.h"
#include "libhpx/debug.h"
#include "libhpx/gas.h"
#include "libhpx/libhpx.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"

#include "commands.h"
#include "peer.h"
#include "pwc.h"
#include "pwc_buffer.h"

typedef struct {
  network_t            vtable;
  uint32_t               rank;
  uint32_t              ranks;
  uint32_t parcel_buffer_size;
  const uint32_t       UNUSED;                  // padding
  gas_t                  *gas;
  size_t          eager_bytes;
  char                 *eager;
  peer_t                peers[];
} pwc_network_t;

static const char *_pwc_id() {
  return "Photon put-with-completion\n";
}

static void _pwc_delete(network_t *network) {
  if (!network) {
    return;
  }

  pwc_network_t *pwc = (pwc_network_t*)network;
  for (int i = 0; i < pwc->ranks; ++i) {
    peer_t *peer = &pwc->peers[i];
    if (i == pwc->rank) {
      segment_fini(&peer->segments[SEGMENT_NULL]);
      segment_fini(&peer->segments[SEGMENT_HEAP]);
      segment_fini(&peer->segments[SEGMENT_PEERS]);
      segment_fini(&peer->segments[SEGMENT_EAGER]);
    }
    peer_fini(peer);
  }

  if (pwc->eager) {
    free(pwc->eager);
  }

  free(pwc);
}

static int _pwc_progress(network_t *network) {
  return 0;
}

/// Perform a parcel send operation to an eager buffer.
///
/// This transforms the parcel send operation into a pwc() operation into the
/// parcel buffer on the target peer.
///
/// This is basically exposed directly to the application programmer through the
/// network interface, and could be called concurrently by a number of different
/// threads. It does not perform any internal locking, as it is merely finding
/// the right peer structure to send through.
///
/// @param      network The network object.
/// @param            p The parcel to send.
///
/// @returns  LIBHPX_OK The operation completed successfully.
///        LIBHPX_ERROR The operation produced an error.
static int _pwc_send(network_t *network, hpx_parcel_t *p) {
  pwc_network_t *pwc = (void*)network;
  int rank = gas_owner_of(pwc->gas, p->target);
  peer_t *peer = &pwc->peers[rank];
  return peer_send(peer, p, HPX_NULL);
}

/// Perform a put-with-command operation to a global heap address.
///
/// This simply the global address into a symmetric-heap offset, finds the
/// peer for the request, and forwards to the p2p put operation.
static int _pwc_pwc(network_t *network,
                    hpx_addr_t to, const void *lva, size_t n,
                    hpx_addr_t lsync, hpx_addr_t rsync, hpx_action_t op)
{
  pwc_network_t *pwc = (void*)network;
  int rank = gas_owner_of(pwc->gas, to);
  peer_t *peer = &pwc->peers[rank];
  uint64_t offset = gas_offset_of(pwc->gas, to);
  command_t cmd = encode_command(op, offset);
  return peer_pwc(peer, offset, lva, n, lsync, rsync, cmd, SEGMENT_HEAP);
}

/// Perform a put operation to a global heap address.
///
/// This simply forwards to the pwc handler with no remote command.
static int _pwc_put(network_t *network, hpx_addr_t to, const void *from,
                    size_t n, hpx_addr_t lsync, hpx_addr_t rsync)
{
  return _pwc_pwc(network, to, from, n, lsync, rsync, HPX_NULL);
}

/// Perform a get operation to a global heap address.
///
/// This simply the global address into a symmetric-heap offset, finds the
/// peer for the request, and forwards to the p2p get operation.
static int _pwc_get(network_t *network, void *lva, hpx_addr_t from, size_t n,
                    hpx_addr_t lsync)
{
  pwc_network_t *pwc = (void*)network;
  int rank = gas_owner_of(pwc->gas, from);
  peer_t *peer = pwc->peers + rank;
  uint64_t offset = gas_offset_of(pwc->gas, from);
  command_t cmd = encode_command(hpx_lco_set_action, lsync);
  return peer_get(peer, lva, n, offset, cmd, SEGMENT_HEAP);
}

static int _probe_local(int rank, uint64_t *op) {
  int flag = 0;
  int e = photon_probe_completion(rank, &flag, op, PHOTON_PROBE_EVQ);
  if (PHOTON_OK != e) {
    dbg_error("photon probe error\n");
  }
  return flag;
}

static int _probe_completion(int rank, uint64_t *op) {
  int flag = 0;
  int e = photon_probe_completion(rank, &flag, op, PHOTON_PROBE_LEDGER);
  if (PHOTON_OK != e) {
    dbg_error("photon probe error\n");
  }
  return flag;
}

static hpx_parcel_t *_pwc_probe(network_t *network, int nrx) {
  pwc_network_t *pwc = (void*)network;
  int rank = pwc->rank;
  hpx_parcel_t *parcels = NULL;

  // each time through the loop, we deal with local command completions
  command_t command;
  while (_probe_local(rank, &command)) {
    hpx_addr_t addr;
    hpx_action_t op;
    decode_command(command, &op, &addr);
    log_net("extracted local interrupt of %s\n", dbg_straction(op));
    int e = hpx_call(addr, op, HPX_NULL, NULL, 0);
    if (HPX_SUCCESS != e) {
      dbg_error("failed to process local command");
    }
  }

  // deal with received commands
  for (int i = 0, e = pwc->ranks; i < e; ++i) {
    while (_probe_completion(i, &command)) {
      hpx_addr_t addr;
      hpx_action_t op;
      decode_command(command, &op, &addr);
      log_net("processing command %s from rank %d\n", dbg_straction(op),
                  i);
      int e = hpx_call(addr, op, HPX_NULL, &i, sizeof(i));
      if (HPX_SUCCESS != e) {
        dbg_error("failed to process local command");
      }
    }
  }

  return parcels;
}

static void _pwc_set_flush(network_t *network) {
}


/// Initialize a peer structure.
///
/// @precondition The peer must already have its segments initialized.
///
/// @param          pwc The network structure.
/// @param         peer The peer to initialize.
///
/// @returns  LIBHPX_OK The peer was initialized successfully.
static int _init_peer(pwc_network_t *pwc, peer_t *peer, int self, int rank) {
  peer->rank = rank;
  int status = pwc_buffer_init(&peer->pwc, rank, 8);
  if (LIBHPX_OK != status) {
    return dbg_error("could not initialize the pwc buffer\n");
  }

  uint32_t size = pwc->parcel_buffer_size;
  status = eager_buffer_init(&peer->rx, peer, rank * size, size);
  if (LIBHPX_OK != status) {
    return dbg_error("could not initialize the parcel rx endpoint\n");
  }

  status = eager_buffer_init(&peer->tx, peer, self * size, size);
  if (LIBHPX_OK != status) {
    return dbg_error("could not initialize the parcel tx endpoint\n");
  }

  status = send_buffer_init(&peer->send, &peer->tx, 8);
  if (LIBHPX_OK != status) {
    return dbg_error("could not initialize the send buffer\n");
  }

  return LIBHPX_OK;
}

peer_t *pwc_get_peer(struct network *network, int src) {
  pwc_network_t *pwc = (void*)network;
  return &pwc->peers[src];
}

network_t *network_pwc_funneled_new(config_t *cfg, boot_t *boot, gas_t *gas,
                                    int nrx)
{
  int e;

  if (boot->type == HPX_BOOT_SMP) {
    log_net("will not instantiate photon for the SMP boot network\n");
    return LIBHPX_OK;
  }

  if (gas->type == HPX_GAS_SMP) {
    log_net("will not instantiate photon for the SMP GAS\n");
    return LIBHPX_OK;
  }

  // Allocate the network object, with enough space for the peer array that
  // contains one peer per rank.
  int ranks = boot_n_ranks(boot);
  pwc_network_t *pwc = malloc(sizeof(*pwc) + ranks * sizeof(peer_t));
  if (!pwc) {
    dbg_error("could not allocate put-with-completion network\n");
    return NULL;
  }

  // Allocate the network's eager segment.
  pwc->eager_bytes = ranks * cfg->parcelbuffersize;
  pwc->eager = malloc(pwc->eager_bytes);
  if (!pwc->eager) {
    dbg_error("malloc(%lu) failed for the eager buffer\n", pwc->eager_bytes);
    goto unwind;
  }

  // Initialize the network's virtual function table.
  pwc->vtable.id = _pwc_id;
  pwc->vtable.delete = _pwc_delete;
  pwc->vtable.progress = _pwc_progress;
  pwc->vtable.send = _pwc_send;
  pwc->vtable.pwc = _pwc_pwc;
  pwc->vtable.put = _pwc_put;
  pwc->vtable.get = _pwc_get;
  pwc->vtable.probe = _pwc_probe;
  pwc->vtable.set_flush = _pwc_set_flush;

  // Store some of the salient information in the network structure.
  pwc->gas = gas;
  pwc->rank = boot_rank(boot);
  pwc->ranks = ranks;
  pwc->parcel_buffer_size = cfg->parcelbuffersize;

  peer_t *local = pwc_get_peer(&pwc->vtable, pwc->rank);
  // Prepare the null segment.
  segment_t *null = &local->segments[SEGMENT_NULL];
  e = segment_init(null, NULL, 0);
  if (LIBHPX_OK != e) {
    dbg_error("could not initialize the NULL segment\n");
    goto unwind;
  }

  // Register the heap segment.
  segment_t *heap = &local->segments[SEGMENT_HEAP];
  e = segment_init(heap, gas_local_base(pwc->gas), gas_local_size(pwc->gas));
  if (LIBHPX_OK != e) {
    dbg_error("could not register the heap segment\n");
    goto unwind;
  }

  // Register the eager segment.
  segment_t *eager = &local->segments[SEGMENT_EAGER];
  e = segment_init(eager, pwc->eager, pwc->eager_bytes);
  if (LIBHPX_OK != e) {
    dbg_error("could not register the eager segment\n");
    goto unwind;
  }

  // Register the peers segment.
  segment_t *peers = &local->segments[SEGMENT_PEERS];
  e = segment_init(peers, (void*)pwc->peers, pwc->ranks * sizeof(peer_t));
  if (LIBHPX_OK != e) {
    dbg_error("could not register the peers segment\n");
    goto unwind;
  }

  // Exchange all of the peer segments.
  e = boot_allgather(boot, local, &pwc->peers, sizeof(*local));
  if (LIBHPX_OK != e) {
    dbg_error("could not exchange peers segment\n");
    goto unwind;
  }

  // Wait until everyone has exchanged segments.
  // NB: is the allgather synchronous? Why are we waiting otherwise...?
  boot_barrier(boot);

  // Initialize each of the peer structures.
  for (int i = 0; i < ranks; ++i) {
    peer_t *peer = pwc_get_peer(&pwc->vtable, i);
    e = _init_peer(pwc, peer, pwc->rank, i);
    if (LIBHPX_OK != e) {
      dbg_error("failed to initialize peer %d of %u\n", i, ranks);
      goto unwind;
    }
  }

  return &pwc->vtable;

 unwind:
  _pwc_delete(&pwc->vtable);
  return NULL;
}

