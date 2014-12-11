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

#include "peer.h"
#include "pwc.h"
#include "pwc_buffer.h"

typedef struct {
  network_t vtable;
  uint32_t    rank;
  uint32_t   ranks;
  gas_t       *gas;
  peer_t     peers[];
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
    pwc_buffer_fini(&pwc->peers[i].puts);
  }
  peer_t *local = pwc->peers + pwc->rank;
  segment_deregister(&local->segments[SEGMENT_HEAP]);
  segment_deregister(&local->segments[SEGMENT_PARCEL]);
  free(local->segments[SEGMENT_PARCEL].base);
  free(pwc);
}


static int _pwc_progress(network_t *network) {
  return 0;
}


/// Perform a parcel-send operation.
static int _pwc_send(network_t *network, hpx_parcel_t *p, hpx_addr_t l) {
  return LIBHPX_EUNIMPLEMENTED;
}


/// Perform a put-with-completion operation to a global heap address.
///
/// This simply the global address into a symmetric-heap offset, finds the
/// peer for the request, and forwards to the p2p put operation.
static int _pwc_pwc(network_t *network, hpx_addr_t to, void *lva, size_t n,
                    hpx_addr_t local, hpx_addr_t remote, hpx_action_t op)
{
  pwc_network_t *pwc = (void*)network;
  int rank = gas_owner_of(pwc->gas, to);
  peer_t *peer = pwc->peers + rank;
  uint64_t offset = gas_offset_of(pwc->gas, to);
  return peer_put(peer, offset, lva, n, local, remote, op, SEGMENT_HEAP);
}


/// Perform a put operation to a global heap address.
///
/// This simply forwards to the pwc handler with no remote completion address.
static int _pwc_put(network_t *network, hpx_addr_t to, void *from, size_t n,
                    hpx_addr_t local, hpx_addr_t remote)
{
  return _pwc_pwc(network, to, from, n, local, remote, HPX_NULL);
}


/// Perform a get operation to a global heap address.
///
/// This simply the global address into a symmetric-heap offset, finds the
/// peer for the request, and forwards to the p2p get operation.
static int _pwc_get(network_t *network, void *lva, hpx_addr_t from, size_t n,
                    hpx_addr_t local)
{
  pwc_network_t *pwc = (void*)network;
  int rank = gas_owner_of(pwc->gas, from);
  peer_t *peer = pwc->peers + rank;
  uint64_t offset = gas_offset_of(pwc->gas, from);
  return peer_get(peer, lva, n, offset, local);
}


static hpx_parcel_t *_pwc_probe(network_t *network, int nrx) {
  return NULL;
}


static void _pwc_set_flush(network_t *network) {
}


network_t *network_pwc_funneled_new(config_t *cfg, boot_t *boot, gas_t *gas,
                                    int nrx)
{
  if (boot->type == HPX_BOOT_SMP) {
    dbg_log_net("will not instantiate photon for the SMP boot network\n");
    goto unwind0;
  }

  if (gas->type == HPX_GAS_SMP) {
    dbg_log_net("will not instantiate photon for the SMP GAS\n");
    goto unwind0;
  }

  int ranks = boot_n_ranks(boot);
  pwc_network_t *pwc = malloc(sizeof(*pwc) + ranks * sizeof(peer_t));
  if (!pwc) {
    dbg_error("could not allocate a put-with-completion network\n");
    goto unwind0;
  }

  pwc->vtable.id = _pwc_id;
  pwc->vtable.delete = _pwc_delete;
  pwc->vtable.progress = _pwc_progress;
  pwc->vtable.send = _pwc_send;
  pwc->vtable.pwc = _pwc_pwc;
  pwc->vtable.put = _pwc_put;
  pwc->vtable.get = _pwc_get;
  pwc->vtable.probe = _pwc_probe;
  pwc->vtable.set_flush = _pwc_set_flush;

  pwc->gas = gas;
  pwc->rank = boot_rank(boot);
  pwc->ranks = ranks;

  peer_t *local = &pwc->peers[pwc->rank];

  // allocate a parcel recv buffer for the parcel segment
  segment_t *parcels = &local->segments[SEGMENT_PARCEL];
  parcels->size = ranks * cfg->parcelbuffersize;
  parcels->base = malloc(parcels->size);
  if (NULL == parcels->base) {
    dbg_error("could not allocate the parcel buffer segment\n");
    goto unwind1;
  }

  // register the parcel segment
  if (LIBHPX_OK != segment_register(parcels)) {
    dbg_error("could not register the parcel buffer segment\n");
    goto unwind2;
  }

  // register the heap segment
  segment_t *heap = &local->segments[SEGMENT_HEAP];
  heap->base = gas_local_base(gas);
  heap->size = gas_local_size(gas);
  if (LIBHPX_OK != segment_register(heap)) {
    dbg_error("could not register the heap segment\n");
    goto unwind3;
  }

  // exchange my segments with my peers
  if (LIBHPX_OK != boot_allgather(boot, local, &pwc->peers, sizeof(*local))) {
    dbg_error("could not exchange peer segments\n");
    goto unwind4;
  }
  boot_barrier(boot);

  // initialize all of the buffers
  int i;
  for (i = 0; i < ranks; ++i) {
    int n = (i != pwc->rank) ? 8 : 0;
    if (LIBHPX_OK != pwc_buffer_init(&pwc->peers[i].puts, i, n)) {
      goto unwind5;
    }
  }

  return &pwc->vtable;

 unwind5:
  for (int j = 0; j < i; ++j) {
    pwc_buffer_fini(&pwc->peers[j].puts);
  }
 unwind4:
  segment_deregister(&local->segments[SEGMENT_HEAP]);
 unwind3:
  segment_deregister(&local->segments[SEGMENT_PARCEL]);
 unwind2:
  free(local->segments[SEGMENT_PARCEL].base);
 unwind1:
  free(pwc);
 unwind0:
  return NULL;
}
