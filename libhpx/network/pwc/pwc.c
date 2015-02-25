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
#include <hpx/builtins.h>

#include "libhpx/action.h"
#include "libhpx/boot.h"
#include "libhpx/config.h"
#include "libhpx/debug.h"
#include "libhpx/gas.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"

#include "commands.h"
#include "parcel_utils.h"
#include "peer.h"
#include "pwc.h"
#include "pwc_buffer.h"

typedef struct pwc_network {
  network_t            vtable;
  uint32_t               rank;
  uint32_t              ranks;
  uint32_t parcel_buffer_size;
  uint32_t parcel_eager_limit;
  gas_t                  *gas;
  size_t          eager_bytes;
  char                 *eager;
  int         flush_on_delete;
  int          UNUSED_PADDING;
  peer_t                peers[];
} pwc_network_t;

/// Utility to wrap a poll completion.
static int _poll(unsigned type, int rank, uint64_t *op, int *remaining) {
  int flag = 0;
  int e = photon_probe_completion(rank, &flag, remaining, op, type);
  if (PHOTON_OK != e) {
    dbg_error("photon probe error\n");
  }
  return flag;
}

static const char *_straction(hpx_action_t id) {
  dbg_assert(here && here->actions);
  return action_table_get_key(here->actions, id);
}

static void _pwc_delete(network_t *network) {
  dbg_assert(network);
  log_error("got here\n");
  pwc_network_t *pwc = (pwc_network_t*)network;

  // Finish up our outstanding rDMA, and then wait. This prevents us from
  // deregistering segments while there are outstanding requests.
  {
    int remaining;
    command_t command;
    int e;
    do {
      e = _poll(PHOTON_PROBE_EVQ, here->rank, &command, &remaining);
    } while (e == PHOTON_OK && remaining > 0);
    log_error("got here\n");
    boot_barrier(here->boot);
  }

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

/// Poll and handle local completions.
static void _probe_local(pwc_network_t *pwc) {
  int rank = pwc->rank;

  // Each time through the loop, we deal with local completions.
  command_t command;
  while (_poll(PHOTON_PROBE_EVQ, rank, &command, NULL)) {
    hpx_addr_t op = command_get_op(command);
    log_net("processing local command: %s\n", _straction(op));
    int e = hpx_xcall(HPX_HERE, op, HPX_NULL, command);
    dbg_assert_str(HPX_SUCCESS == e, "failed to process local command\n");
  }
}

/// Probe for remote completions from @p rank.
///
/// This function claims to return a list of parcels, but it actually processes
/// all messages internally, using the local work-queue directly.
static hpx_parcel_t *_probe(network_t *network, int rank) {
  command_t command;
  while (_poll(PHOTON_PROBE_LEDGER, rank, &command, NULL)) {
    hpx_addr_t addr;
    hpx_action_t op;
    decode_command(command, &op, &addr);
    log_net("processing command %s from rank %d\n", _straction(op), rank);
    int e = hpx_call(addr, op, HPX_NULL, &rank, sizeof(rank));
    dbg_assert_str(HPX_SUCCESS == e, "failed to process command\n");
  }
  return NULL;
}

/// Progress the pwc() network.
///
/// Currently, this processes all outstanding local completions and then probes
/// each potential source for commands. It is not thread safe.
static int _pwc_progress(network_t *network) {
  pwc_network_t *pwc = (void*)network;
  _probe_local(pwc);
  for (int i = 0, e = pwc->ranks; i < e; ++i) {
    _probe(network, i);
  }
  return 0;
}

static hpx_parcel_t *_pwc_probe(network_t *network, int rank) {
  return NULL;
}

/// Perform a parcel send operation.
///
/// This determines which peer the operation is occurring to, and which send
/// protocol to use, and then forwards to the appropriate peer/handler pair.
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
  size_t bytes = pwc_network_size(p);
  if (bytes < pwc->parcel_eager_limit) {
    return peer_send(peer, p, HPX_NULL);
  }
  else {
    return peer_send_rendezvous(peer, p, HPX_NULL);
  }
}

/// Perform a put-with-command operation to a global heap address.
///
/// This simply the global address into a symmetric-heap offset, finds the
/// peer for the request, and forwards to the p2p put operation.
static int _pwc_pwc(network_t *network,
                    hpx_addr_t to, const void *lva, size_t n,
                    hpx_action_t lop, hpx_addr_t laddr,
                    hpx_action_t rop, hpx_addr_t raddr) {
  pwc_network_t *pwc = (void*)network;
  int rank = gas_owner_of(pwc->gas, to);
  peer_t *peer = &pwc->peers[rank];
  uint64_t offset = gas_offset_of(pwc->gas, to);
  command_t lsync = encode_command(lop, laddr);
  command_t rsync = encode_command(rop, raddr);
  return peer_pwc(peer, offset, lva, n, lsync, rsync, SEGMENT_HEAP);
}

/// Perform a put operation to a global heap address.
///
/// This simply forwards to the pwc handler with no remote command.
static int _pwc_put(network_t *net, hpx_addr_t to, const void *from,
                    size_t n, hpx_action_t lop, hpx_addr_t laddr) {
  return _pwc_pwc(net, to, from, n, lop, laddr, HPX_ACTION_NULL, HPX_NULL);
}

/// Perform a get operation to a global heap address.
///
/// This simply the global address into a symmetric-heap offset, finds the
/// peer for the request, and forwards to the p2p get operation.
static int _pwc_get(network_t *network, void *lva, hpx_addr_t from, size_t n,
                    hpx_action_t lop, hpx_addr_t laddr) {
  pwc_network_t *pwc = (void*)network;
  int rank = gas_owner_of(pwc->gas, from);
  peer_t *peer = pwc->peers + rank;
  uint64_t offset = gas_offset_of(pwc->gas, from);
  command_t lsync = encode_command(lop, laddr);
  return peer_get(peer, lva, n, offset, lsync, SEGMENT_HEAP);
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
    return log_error("could not initialize the pwc buffer\n");
  }

  // Figure out where I receive from in my eager buffer w.r.t. this peer.
  uint32_t size = pwc->parcel_buffer_size;
  char *base = pwc->eager + rank * size;
  status = eager_buffer_init(&peer->rx, peer, 0, base, size);
  if (LIBHPX_OK != status) {
    return log_error("could not initialize the parcel rx endpoint\n");
  }

  // Figure out where I send to w.r.t. this peer in their eager buffer.
  uint32_t offset = self * size;
  status = eager_buffer_init(&peer->tx, peer, offset, NULL, size);
  if (LIBHPX_OK != status) {
    return log_error("could not initialize the parcel tx endpoint\n");
  }

  status = send_buffer_init(&peer->send, &peer->tx, 8);
  if (LIBHPX_OK != status) {
    return log_error("could not initialize the send buffer\n");
  }

  return LIBHPX_OK;
}

peer_t *pwc_get_peer(int src) {
  dbg_assert(here && here->network);
  pwc_network_t *pwc = (void*)here->network;
  return &pwc->peers[src];
}

network_t *network_pwc_funneled_new(config_t *cfg, boot_t *boot, gas_t *gas,
                                    int nrx) {
  int e;

  if (boot->type == HPX_BOOT_SMP) {
    log_net("will not instantiate photon for the SMP boot network\n");
    return NULL;
  }

  if (gas->type == HPX_GAS_SMP) {
    log_net("will not instantiate photon for the SMP GAS\n");
    return NULL;
  }

  // Allocate the network object, with enough space for the peer array that
  // contains one peer per rank.
  int ranks = boot_n_ranks(boot);
  pwc_network_t *pwc = malloc(sizeof(*pwc) + ranks * sizeof(peer_t));
  if (!pwc) {
    dbg_error("could not allocate put-with-completion network\n");
  }

  // Store some of the salient information in the network structure.
  pwc->gas = gas;
  pwc->rank = boot_rank(boot);
  pwc->ranks = ranks;
  pwc->parcel_eager_limit = 1u << ceil_log2_32(cfg->pwc_parceleagerlimit);
  // NB: 16 is sizeof(_get_parcel_args_t) in peer.c
  const int limit = sizeof(hpx_parcel_t) - pwc_prefix_size() + 16;
  if (pwc->parcel_eager_limit < limit) {
    log_error("--hpx-parceleagerlimit must be at least %u bytes\n", limit);
    goto unwind;
  }

  pwc->parcel_buffer_size = 1u << ceil_log2_32(cfg->pwc_parcelbuffersize);
  log("initialized a %u-byte eager buffer\n", pwc->parcel_buffer_size);

  // Allocate the network's eager segment.
  pwc->eager_bytes = ranks * pwc->parcel_buffer_size;
  pwc->eager = malloc(pwc->eager_bytes);
  if (!pwc->eager) {
    log_error("malloc(%lu) failed for the eager buffer\n", pwc->eager_bytes);
    goto unwind;
  }

  // Initialize the network's virtual function table.
  pwc->vtable.type = LIBHPX_NETWORK_PWC;
  pwc->vtable.delete = _pwc_delete;
  pwc->vtable.progress = _pwc_progress;
  pwc->vtable.send = _pwc_send;
  pwc->vtable.pwc = _pwc_pwc;
  pwc->vtable.put = _pwc_put;
  pwc->vtable.get = _pwc_get;
  pwc->vtable.probe = _pwc_probe;
  pwc->vtable.set_flush = _pwc_set_flush;


  if (pwc->parcel_eager_limit > pwc->parcel_buffer_size) {
    dbg_error(" --hpx-parceleagerlimit (%u) must be less than "
              "--hpx-parcelbuffersize (%u)\n",
              pwc->parcel_eager_limit, pwc->parcel_buffer_size);
  }

  peer_t local;
  // Prepare the null segment.
  segment_t *null = &local.segments[SEGMENT_NULL];
  e = segment_init(null, NULL, 0);
  if (LIBHPX_OK != e) {
    log_error("could not initialize the NULL segment\n");
    goto unwind;
  }

  // Register the heap segment.
  segment_t *heap = &local.segments[SEGMENT_HEAP];
  e = segment_init(heap, gas_local_base(pwc->gas), gas_local_size(pwc->gas));
  if (LIBHPX_OK != e) {
    log_error("could not register the heap segment\n");
    goto unwind;
  }

  // Register the eager segment.
  segment_t *eager = &local.segments[SEGMENT_EAGER];
  e = segment_init(eager, pwc->eager, pwc->eager_bytes);
  if (LIBHPX_OK != e) {
    log_error("could not register the eager segment\n");
    goto unwind;
  }

  // Register the peers segment.
  segment_t *peers = &local.segments[SEGMENT_PEERS];
  e = segment_init(peers, (void*)pwc->peers, pwc->ranks * sizeof(peer_t));
  if (LIBHPX_OK != e) {
    log_error("could not register the peers segment\n");
    goto unwind;
  }

  // Exchange all of the peer segments.
  e = boot_allgather(boot, &local, &pwc->peers, sizeof(local));
  if (LIBHPX_OK != e) {
    log_error("could not exchange peers segment\n");
    goto unwind;
  }

  // Wait until everyone has exchanged segments.
  // NB: is the allgather synchronous? Why are we waiting otherwise...?
  boot_barrier(boot);

  // Initialize each of the peer structures.
  for (int i = 0; i < ranks; ++i) {
    peer_t *peer = &pwc->peers[i];
    int e = _init_peer(pwc, peer, pwc->rank, i);
    if (LIBHPX_OK != e) {
      log_error("failed to initialize peer %d of %u\n", i, ranks);
      goto unwind;
    }
  }

  return &pwc->vtable;

 unwind:
  _pwc_delete(&pwc->vtable);
  return NULL;
}
