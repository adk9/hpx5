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

#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/network.h>

#include "pwc.h"

typedef struct {
  network_t vtable;
} _pwc_t;

static const char *_photon_id() {
  return "Photon put-with-completion\n";
}

static void _photon_delete(network_t *network) {
  if (!network) {
    return;
  }
}

static int _photon_progress(network_t *network) {
  return 0;
}

static int _photon_send(network_t *network, hpx_parcel_t *p,
                          hpx_addr_t l)
{
  return LIBHPX_EUNIMPLEMENTED;
}


static int _photon_pwc(network_t *network,
                         hpx_addr_t to, void *from, size_t n,
                         hpx_addr_t local, hpx_addr_t remote, hpx_action_t op)
{
  return LIBHPX_EUNIMPLEMENTED;
}


static int _photon_put(network_t *network,
                         hpx_addr_t to, void *from, size_t n,
                         hpx_addr_t local, hpx_addr_t remote)
{
  return LIBHPX_EUNIMPLEMENTED;
}


static int _photon_get(network_t *network,
                         void *to, hpx_addr_t from, size_t n,
                         hpx_addr_t local)
{
  return LIBHPX_EUNIMPLEMENTED;
}


static hpx_parcel_t *_photon_probe(network_t *network, int nrx) {
  return NULL;
}


static void _photon_set_flush(network_t *network) {
}


network_t *network_pwc_funneled_new(struct gas_class *gas, int nrx) {
  _pwc_t *photon =  malloc(sizeof(*photon));
  if (!photon) {
    dbg_error("could not allocate a Photon put-with-completion network\n");
    return NULL;
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

  return &photon->vtable;
}
