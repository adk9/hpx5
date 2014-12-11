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

#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/network.h>

#include "pwc.h"
#include "pwc_buffer.h"

typedef struct {
  network_t     vtable;
  gas_t           *gas;
  int             rank;
  int           UNUSED;
  pwc_buffer_t buffers[];
} _pwc_t;


static const char *_photon_id() {
  return "Photon put-with-completion\n";
}


static void _photon_delete(network_t *network) {
  if (!network) {
    return;
  }

  _pwc_t *pwc = (_pwc_t*)network;
  size_t size = gas_local_size(pwc->gas);
  void *base = gas_local_base(pwc->gas);
  if (PHOTON_OK != photon_unregister_buffer(base, size)) {
    dbg_log_net("could not unregister the local heap segment %p\n", base);
  }

  free(pwc);
}


static int _photon_progress(network_t *network) {
  return 0;
}


static int _photon_send(network_t *network, hpx_parcel_t *p, hpx_addr_t l) {
  return LIBHPX_EUNIMPLEMENTED;
}


static int _photon_pwc(network_t *network, hpx_addr_t to, void *lva, size_t n,
                       hpx_addr_t local, hpx_addr_t remote, hpx_action_t op)
{
  _pwc_t *pwc = (void*)network;
  int rank = gas_owner_of(pwc->gas, to);
  void *rva = pwc->buffers[rank].base + gas_offset_of(pwc->gas, to);
  return pwc_buffer_pwc(pwc->buffers + rank, rva, lva, n, local, remote, op);
}


static int _photon_put(network_t *network, hpx_addr_t to, void *from, size_t n,
                       hpx_addr_t local, hpx_addr_t remote)
{

  return _photon_pwc(network, to, from, n, local, remote, HPX_NULL);
}


static int _photon_get(network_t *network, void *lva, hpx_addr_t from, size_t n,
                       hpx_addr_t local)
{
  _pwc_t *pwc = (void*)network;

  int flags = (local != HPX_NULL) ? PHOTON_REQ_ONE_CQE : PHOTON_REQ_NO_CQE;
  int rank = gas_owner_of(pwc->gas, from);
  const void *rva = pwc->buffers[rank].base + gas_offset_of(pwc->gas, from);
  void *vrva = (void*)rva;
  struct photon_buffer_priv_t key = pwc->buffers[rank].key;

  int e = photon_get_with_completion(rank, lva, n, vrva, key, local, flags);
  if (PHOTON_OK != e) {
    return dbg_error("could not initiate a get()\n");
  }

  return LIBHPX_OK;
}


static hpx_parcel_t *_photon_probe(network_t *network, int nrx) {
  return NULL;
}


static void _photon_set_flush(network_t *network) {
}


network_t *network_pwc_funneled_new(boot_t *boot, gas_t *gas, int nrx) {
  if (boot->type == HPX_BOOT_SMP) {
    dbg_log_net("will not instantiate photon for the SMP boot network\n");
    goto unwind0;
  }

  if (gas->type == HPX_GAS_SMP) {
    dbg_log_net("will not instantiate photon for the SMP GAS\n");
    goto unwind0;
  }

  int ranks = boot_n_ranks(boot);
  _pwc_t *photon = malloc(sizeof(*photon) + ranks * sizeof(pwc_buffer_t));
  if (!photon) {
    dbg_error("could not allocate a Photon put-with-completion network\n");
    goto unwind0;
  }

  photon->vtable.id = _photon_id;
  photon->vtable.delete = _photon_delete;
  photon->vtable.progress = _photon_progress;
  photon->vtable.send = _photon_send;
  photon->vtable.pwc = _photon_pwc;
  photon->vtable.put = _photon_put;
  photon->vtable.get = _photon_get;
  photon->vtable.probe = _photon_probe;
  photon->vtable.set_flush = _photon_set_flush;

  photon->gas = gas;
  photon->rank = boot_rank(boot);

  // Register the local heap segment.
  size_t size = gas_local_size(gas);
  void *base = gas_local_base(gas);

  if (PHOTON_OK != photon_register_buffer(base, size)) {
    dbg_error("failed to register the local heap segment with Photon\n");
    goto unwind1;
  }
  else {
    dbg_log_net("registered the local segment (%p, %lu)\n", base, size);
  }

  struct photon_buffer_priv_t key;
  if (PHOTON_OK != photon_get_buffer_private(base, size , &key)) {
    dbg_error("failed to get the local segment access key from Photon\n");
    goto unwind2;
  }
  pwc_buffer_t *local = photon->buffers + photon->rank;
  local->base = base;
  local->key = key;
  local->rank = photon->rank;

  if (LIBHPX_OK != boot_allgather(boot, local, &photon->buffers, sizeof(*local))) {
    dbg_error("could not exchange heap segments\n");
    goto unwind2;
  }

  // wait for the exchange to happen on all localities
  boot_barrier(boot);

  assert(local->base == base);
  assert(local->key.key0 == key.key0);
  assert(local->key.key1 == key.key1);

  for (int i = 0; i < ranks; ++i) {
    pwc_buffer_init(photon->buffers + i, i, 32);
  }

  return &photon->vtable;

 unwind2:
  photon_unregister_buffer(base, size);
 unwind1:
  free(photon);
 unwind0:
  return NULL;
}
